#include "Domain/Package/Internal/Lzma1Decoder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <QCoreApplication>
#include <QString>

#include "Core/Error/ErrorCode.h"

namespace Domain::Package::Internal {
namespace {

constexpr std::size_t kValveHeaderSize = 17;
constexpr std::size_t kLzmaPropsSize = 5;
constexpr std::size_t kRangeInitBytes = 5;
constexpr std::size_t kMaxUncompressedSize = 512ULL * 1024ULL * 1024ULL; // 512 MiB OOM safety guard

// Range coder constants
constexpr std::uint32_t kNumTopBits = 24;
constexpr std::uint32_t kTopValue = 1U << kNumTopBits;
constexpr std::uint32_t kNumBitModelTotalBits = 11;
constexpr std::uint32_t kBitModelTotal = 1U << kNumBitModelTotalBits;
constexpr std::uint32_t kNumMoveBits = 5;
constexpr std::uint16_t kProbInit = kBitModelTotal / 2;

// LZMA1 model constants
constexpr std::uint32_t kNumPosBitsMax = 4;
constexpr std::uint32_t kNumPosStatesMax = 1U << kNumPosBitsMax;
constexpr std::uint32_t kNumStates = 12;
constexpr std::uint32_t kNumLenToPosStates = 4;
constexpr std::uint32_t kNumAlignBits = 4;
constexpr std::uint32_t kAlignTableSize = 1U << kNumAlignBits;
constexpr std::uint32_t kStartPosModelIndex = 4;
constexpr std::uint32_t kEndPosModelIndex = 14;
constexpr std::uint32_t kNumFullDistances = 1U << (kEndPosModelIndex / 2);
constexpr std::uint32_t kNumSpecPosProbs = kNumFullDistances - kEndPosModelIndex; // 114
constexpr std::uint32_t kNumPosSlotBits = 6;
constexpr std::uint32_t kPosSlotTableSize = 1U << kNumPosSlotBits; // 64
constexpr std::uint32_t kMatchMinLen = 2;

// LenDecoder table layout
constexpr std::size_t kLenChoiceOffset = 0;
constexpr std::size_t kLenChoice2Offset = 1;
constexpr std::size_t kLenLowOffset = 2;
constexpr std::size_t kLenLowSize = kNumPosStatesMax * 8; // 128
constexpr std::size_t kLenMidOffset = kLenLowOffset + kLenLowSize;
constexpr std::size_t kLenMidSize = kNumPosStatesMax * 8; // 128
constexpr std::size_t kLenHighOffset = kLenMidOffset + kLenMidSize;
constexpr std::size_t kLenHighSize = 256;
constexpr std::size_t kLenTableSize = kLenHighOffset + kLenHighSize; // 514

// Flat probability array offsets
constexpr std::size_t kIsMatchOffset = 0;
constexpr std::size_t kIsRepOffset = kIsMatchOffset + kNumStates * kNumPosStatesMax;
constexpr std::size_t kIsRepG0Offset = kIsRepOffset + kNumStates;
constexpr std::size_t kIsRepG1Offset = kIsRepG0Offset + kNumStates;
constexpr std::size_t kIsRepG2Offset = kIsRepG1Offset + kNumStates;
constexpr std::size_t kIsRep0LongOffset = kIsRepG2Offset + kNumStates;
constexpr std::size_t kPosSlotOffset = kIsRep0LongOffset + kNumStates * kNumPosStatesMax;
constexpr std::size_t kSpecPosOffset = kPosSlotOffset + kNumLenToPosStates * kPosSlotTableSize;
constexpr std::size_t kAlignOffset = kSpecPosOffset + kNumSpecPosProbs;
constexpr std::size_t kMatchLenOffset = kAlignOffset + kAlignTableSize;
constexpr std::size_t kRepLenOffset = kMatchLenOffset + kLenTableSize;
constexpr std::size_t kLiteralOffset = kRepLenOffset + kLenTableSize;

constexpr std::uint32_t kLiteralSubcoderSize = 0x300;
constexpr std::uint32_t kEndOfStreamDist = 0xFFFFFFFFU;

std::uint32_t readU32LE(const std::byte* ptr) noexcept
{
    return static_cast<std::uint32_t>(static_cast<std::uint8_t>(ptr[0]))
        | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(ptr[1])) << 8)
        | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(ptr[2])) << 16)
        | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(ptr[3])) << 24);
}

struct LzmaProps {
    std::uint32_t lc = 0;
    std::uint32_t lp = 0;
    std::uint32_t pb = 0;
    std::uint32_t dictSize = 0;
};

bool parseProps(std::span<const std::byte> props5, LzmaProps& out) noexcept
{
    if (props5.size() != kLzmaPropsSize) {
        return false;
    }

    std::uint32_t d = static_cast<std::uint8_t>(props5[0]);
    if (d >= 9U * 5U * 5U) {
        return false;
    }

    out.lc = d % 9U;
    d /= 9U;
    out.lp = d % 5U;
    out.pb = d / 5U;

    if (out.lc > 8U || out.lp > 4U || out.pb > 4U) {
        return false;
    }

    out.dictSize = std::max<std::uint32_t>(readU32LE(props5.data() + 1), 4096U);
    return true;
}

class RangeDecoder {
public:
    explicit RangeDecoder(std::span<const std::byte> stream) noexcept
        : m_stream(stream)
    {
    }

    bool init() noexcept
    {
        if (m_stream.size() < kRangeInitBytes) {
            return false;
        }
        m_code = 0;
        for (std::size_t i = 0; i < kRangeInitBytes; ++i) {
            m_code = (m_code << 8) | static_cast<std::uint8_t>(m_stream[m_pos++]);
        }
        m_range = 0xFFFFFFFFU;
        return true;
    }

    bool decodeBit(std::uint16_t& prob, std::uint32_t& outBit) noexcept
    {
        if (!normalize()) {
            return false;
        }

        const std::uint32_t bound = (m_range >> kNumBitModelTotalBits) * prob;
        if (m_code < bound) {
            m_range = bound;
            prob = static_cast<std::uint16_t>(prob + ((kBitModelTotal - prob) >> kNumMoveBits));
            outBit = 0;
        } else {
            m_range -= bound;
            m_code -= bound;
            prob = static_cast<std::uint16_t>(prob - (prob >> kNumMoveBits));
            outBit = 1;
        }
        return true;
    }

    bool decodeDirectBit(std::uint32_t& outBit) noexcept
    {
        if (!normalize()) {
            return false;
        }

        m_range >>= 1;
        if (m_code >= m_range) {
            m_code -= m_range;
            outBit = 1;
        } else {
            outBit = 0;
        }
        return true;
    }

    bool decodeBitTree(std::uint16_t* treeProbs, int numBits, std::uint32_t& outSymbol) noexcept
    {
        std::uint32_t m = 1;
        for (int i = 0; i < numBits; ++i) {
            std::uint32_t bit = 0;
            if (!decodeBit(treeProbs[m], bit)) {
                return false;
            }
            m = (m << 1) | bit;
        }
        outSymbol = m - (1U << numBits);
        return true;
    }

    bool decodeReverseBitTree(std::uint16_t* treeProbs, int numBits, std::uint32_t& outSymbol) noexcept
    {
        std::uint32_t m = 1;
        std::uint32_t symbol = 0;
        for (int bitIndex = 0; bitIndex < numBits; ++bitIndex) {
            std::uint32_t bit = 0;
            if (!decodeBit(treeProbs[m], bit)) {
                return false;
            }
            m = (m << 1) | bit;
            symbol |= (bit << bitIndex);
        }
        outSymbol = symbol;
        return true;
    }

    bool decodeSpecPos(
        std::uint16_t* specPosProbs,
        std::size_t baseOffset,
        int numDirectBits,
        std::uint32_t& outSymbol) noexcept
    {
        std::uint32_t m = 1;
        std::uint32_t symbol = 0;
        for (int bitIndex = 0; bitIndex < numDirectBits; ++bitIndex) {
            const std::size_t probIndex = baseOffset + m - 1;
            if (probIndex >= kNumSpecPosProbs) {
                return false;
            }
            std::uint32_t bit = 0;
            if (!decodeBit(specPosProbs[probIndex], bit)) {
                return false;
            }
            m = (m << 1) | bit;
            symbol |= (bit << bitIndex);
        }
        outSymbol = symbol;
        return true;
    }

    bool decodeLength(std::uint16_t* lenProbs, std::uint32_t posState, std::uint32_t& outLen) noexcept
    {
        std::uint32_t choice = 0;
        if (!decodeBit(lenProbs[kLenChoiceOffset], choice)) {
            return false;
        }
        if (choice == 0) {
            std::uint32_t sym = 0;
            if (!decodeBitTree(lenProbs + kLenLowOffset + (posState * 8U), 3, sym)) {
                return false;
            }
            outLen = sym;
            return true;
        }

        std::uint32_t choice2 = 0;
        if (!decodeBit(lenProbs[kLenChoice2Offset], choice2)) {
            return false;
        }
        if (choice2 == 0) {
            std::uint32_t sym = 0;
            if (!decodeBitTree(lenProbs + kLenMidOffset + (posState * 8U), 3, sym)) {
                return false;
            }
            outLen = 8U + sym;
            return true;
        }

        std::uint32_t sym = 0;
        if (!decodeBitTree(lenProbs + kLenHighOffset, 8, sym)) {
            return false;
        }
        outLen = 16U + sym;
        return true;
    }

private:
    bool normalize() noexcept
    {
        if (m_range < kTopValue) {
            if (m_pos >= m_stream.size()) {
                return false;
            }
            m_range <<= 8;
            m_code = (m_code << 8) | static_cast<std::uint8_t>(m_stream[m_pos++]);
        }
        return true;
    }

    std::span<const std::byte> m_stream;
    std::size_t m_pos = 0;
    std::uint32_t m_range = 0xFFFFFFFFU;
    std::uint32_t m_code = 0;
};

constexpr std::uint32_t nextLiteralState(std::uint32_t state) noexcept
{
    if (state < 4U) {
        return 0U;
    }
    if (state < 10U) {
        return state - 3U;
    }
    return state - 6U;
}

constexpr std::uint32_t nextMatchState(std::uint32_t state) noexcept
{
    return (state < 7U) ? 7U : 10U;
}

constexpr std::uint32_t nextRepState(std::uint32_t state) noexcept
{
    return (state < 7U) ? 8U : 11U;
}

constexpr std::uint32_t nextShortRepState(std::uint32_t state) noexcept
{
    return (state < 7U) ? 9U : 11U;
}

Core::Result<std::vector<std::byte>> corruptedError(const QString& message)
{
    return Core::Result<std::vector<std::byte>>::failure(
        Core::Error::ErrorCode::CorruptedData,
        message);
}

} // namespace

bool Lzma1Decoder::hasValveLzmaMagic(std::span<const std::byte> data) noexcept
{
    if (data.size() < 4) {
        return false;
    }
    return readU32LE(data.data()) == kValveLzmaMagic;
}

Core::Result<std::vector<std::byte>> Lzma1Decoder::decompressRaw(
    std::span<const std::byte> props5,
    std::span<const std::byte> compressedStream,
    std::size_t uncompressedSize)
{
    LzmaProps props;
    if (!parseProps(props5, props)) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("Lzma1Decoder", "Invalid LZMA1 properties header"));
    }

    if (uncompressedSize > kMaxUncompressedSize) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::OutOfMemory,
            QCoreApplication::translate("Lzma1Decoder", "LZMA uncompressed size exceeds safety limit"),
            QStringLiteral("uncompressedSize=%1, limit=%2")
                .arg(uncompressedSize)
                .arg(kMaxUncompressedSize));
    }

    if (uncompressedSize == 0) {
        return Core::Result<std::vector<std::byte>>::success(std::vector<std::byte>{});
    }

    RangeDecoder rc(compressedStream);
    if (!rc.init()) {
        return corruptedError(
            QCoreApplication::translate("Lzma1Decoder", "LZMA1 compressed stream is truncated"));
    }

    const std::size_t numLiteralStates = std::size_t{1} << (props.lc + props.lp);
    const std::size_t totalProbs = kLiteralOffset + numLiteralStates * kLiteralSubcoderSize;
    std::vector<std::uint16_t> probs(totalProbs, kProbInit);

    std::vector<std::byte> out;
    out.reserve(uncompressedSize);

    const std::uint32_t posMask = (1U << props.pb) - 1U;
    const std::uint32_t lpMask = (1U << props.lp) - 1U;

    std::uint32_t state = 0;
    std::uint32_t rep0 = 1;
    std::uint32_t rep1 = 1;
    std::uint32_t rep2 = 1;
    std::uint32_t rep3 = 1;

    while (out.size() < uncompressedSize) {
        const std::uint32_t posState = static_cast<std::uint32_t>(out.size()) & posMask;

        std::uint32_t isMatchBit = 0;
        if (!rc.decodeBit(probs[kIsMatchOffset + state * kNumPosStatesMax + posState], isMatchBit)) {
            return corruptedError(
                QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 stream"));
        }

        if (isMatchBit == 0) {
            // Literal
            const std::uint8_t prevByte = out.empty() ? 0U : static_cast<std::uint8_t>(out.back());
            const std::size_t litState = ((static_cast<std::size_t>(out.size()) & lpMask) << props.lc)
                + (static_cast<std::uint32_t>(prevByte) >> (8U - props.lc));
            std::uint16_t* litProbs = probs.data() + kLiteralOffset + litState * kLiteralSubcoderSize;

            std::uint32_t symbol = 1;
            if (state < 7U) {
                do {
                    std::uint32_t bit = 0;
                    if (!rc.decodeBit(litProbs[symbol], bit)) {
                        return corruptedError(
                            QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 literal stream"));
                    }
                    symbol = (symbol << 1) | bit;
                } while (symbol < 0x100U);
            } else {
                if (rep0 == 0U || static_cast<std::size_t>(rep0) > out.size()) {
                    return corruptedError(
                        QCoreApplication::translate("Lzma1Decoder", "Invalid LZMA1 match distance in literal state"));
                }
                std::uint32_t matchByte = static_cast<std::uint8_t>(out[out.size() - rep0]);
                std::uint32_t offs = 0x100U;
                do {
                    matchByte <<= 1;
                    const std::uint32_t matchBit = matchByte & offs;
                    std::uint32_t bit = 0;
                    if (!rc.decodeBit(litProbs[offs + matchBit + symbol], bit)) {
                        return corruptedError(
                            QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 matched literal stream"));
                    }
                    symbol = (symbol << 1) | bit;
                    offs &= ~(matchBit ^ (bit << 8));
                } while (symbol < 0x100U);
            }

            out.push_back(static_cast<std::byte>(symbol - 0x100U));
            state = nextLiteralState(state);
            continue;
        }

        // Match or Rep
        std::uint32_t isRepBit = 0;
        if (!rc.decodeBit(probs[kIsRepOffset + state], isRepBit)) {
            return corruptedError(
                QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 stream"));
        }

        std::uint32_t len = 0;
        if (isRepBit == 0) {
            // Simple Match
            rep3 = rep2;
            rep2 = rep1;
            rep1 = rep0;

            if (!rc.decodeLength(probs.data() + kMatchLenOffset, posState, len)) {
                return corruptedError(
                    QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 match length"));
            }
            len += kMatchMinLen;
            state = nextMatchState(state);

            const std::uint32_t lenToPosState = std::min<std::uint32_t>(len - kMatchMinLen, kNumLenToPosStates - 1U);
            std::uint32_t posSlot = 0;
            if (!rc.decodeBitTree(probs.data() + kPosSlotOffset + lenToPosState * kPosSlotTableSize, kNumPosSlotBits, posSlot)) {
                return corruptedError(
                    QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 position slot"));
            }

            std::uint32_t dist = 0;
            if (posSlot < kStartPosModelIndex) {
                dist = posSlot;
            } else {
                const int numDirectBits = static_cast<int>((posSlot >> 1) - 1U);
                dist = (2U | (posSlot & 1U)) << numDirectBits;
                if (posSlot < kEndPosModelIndex) {
                    std::uint32_t specSym = 0;
                    if (!rc.decodeSpecPos(probs.data() + kSpecPosOffset, dist - posSlot, numDirectBits, specSym)) {
                        return corruptedError(
                            QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 distance bits"));
                    }
                    dist += specSym;
                } else {
                    std::uint32_t directBits = 0;
                    const int directCount = numDirectBits - static_cast<int>(kNumAlignBits);
                    for (int i = 0; i < directCount; ++i) {
                        std::uint32_t bit = 0;
                        if (!rc.decodeDirectBit(bit)) {
                            return corruptedError(
                                QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 direct distance bits"));
                        }
                        directBits = (directBits << 1) | bit;
                    }
                    std::uint32_t alignSym = 0;
                    if (!rc.decodeReverseBitTree(probs.data() + kAlignOffset, static_cast<int>(kNumAlignBits), alignSym)) {
                        return corruptedError(
                            QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 align bits"));
                    }
                    dist |= (directBits << kNumAlignBits) | alignSym;
                }
            }

            if (dist == kEndOfStreamDist) {
                break;
            }
            if (static_cast<std::size_t>(dist) >= out.size()) {
                return corruptedError(
                    QCoreApplication::translate("Lzma1Decoder", "LZMA1 match distance exceeds decoded output"));
            }
            rep0 = dist + 1U;
        } else {
            // Rep Match
            std::uint32_t isRepG0Bit = 0;
            if (!rc.decodeBit(probs[kIsRepG0Offset + state], isRepG0Bit)) {
                return corruptedError(
                    QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 repG0 flag"));
            }

            if (isRepG0Bit == 0) {
                std::uint32_t isRep0LongBit = 0;
                if (!rc.decodeBit(probs[kIsRep0LongOffset + state * kNumPosStatesMax + posState], isRep0LongBit)) {
                    return corruptedError(
                        QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 rep0Long flag"));
                }
                if (isRep0LongBit == 0) {
                    // ShortRep (single byte at rep0)
                    if (rep0 == 0U || static_cast<std::size_t>(rep0) > out.size()) {
                        return corruptedError(
                            QCoreApplication::translate("Lzma1Decoder", "Invalid LZMA1 short-rep distance"));
                    }
                    out.push_back(out[out.size() - rep0]);
                    state = nextShortRepState(state);
                    continue;
                }
            } else {
                std::uint32_t isRepG1Bit = 0;
                if (!rc.decodeBit(probs[kIsRepG1Offset + state], isRepG1Bit)) {
                    return corruptedError(
                        QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 repG1 flag"));
                }
                std::uint32_t distance = 0;
                if (isRepG1Bit == 0) {
                    distance = rep1;
                } else {
                    std::uint32_t isRepG2Bit = 0;
                    if (!rc.decodeBit(probs[kIsRepG2Offset + state], isRepG2Bit)) {
                        return corruptedError(
                            QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 repG2 flag"));
                    }
                    if (isRepG2Bit == 0) {
                        distance = rep2;
                    } else {
                        distance = rep3;
                        rep3 = rep2;
                    }
                    rep2 = rep1;
                }
                rep1 = rep0;
                rep0 = distance;
            }

            if (!rc.decodeLength(probs.data() + kRepLenOffset, posState, len)) {
                return corruptedError(
                    QCoreApplication::translate("Lzma1Decoder", "Unexpected end of LZMA1 rep length"));
            }
            len += kMatchMinLen;
            state = nextRepState(state);
        }

        if (rep0 == 0U || static_cast<std::size_t>(rep0) > out.size()) {
            return corruptedError(
                QCoreApplication::translate("Lzma1Decoder", "LZMA1 back-reference distance exceeds decoded output"));
        }
        if (static_cast<std::size_t>(len) > uncompressedSize - out.size()) {
            return corruptedError(
                QCoreApplication::translate("Lzma1Decoder", "LZMA1 match length exceeds expected uncompressed size"));
        }

        for (std::uint32_t i = 0; i < len; ++i) {
            out.push_back(out[out.size() - rep0]);
        }
    }

    if (out.size() != uncompressedSize) {
        return corruptedError(
            QCoreApplication::translate("Lzma1Decoder", "LZMA1 stream ended before reaching expected uncompressed size"));
    }

    return Core::Result<std::vector<std::byte>>::success(std::move(out));
}

Core::Result<std::vector<std::byte>> Lzma1Decoder::decompressValveLump(
    std::span<const std::byte> valveLzmaData)
{
    if (valveLzmaData.size() < kValveHeaderSize) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("Lzma1Decoder", "Valve LZMA data is shorter than the 17-byte header"));
    }

    if (!hasValveLzmaMagic(valveLzmaData)) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("Lzma1Decoder", "Invalid Valve LZMA magic signature"));
    }

    const std::uint32_t actualSize = readU32LE(valveLzmaData.data() + 4);
    const std::uint32_t lzmaSize = readU32LE(valveLzmaData.data() + 8);
    const std::span<const std::byte> props5 = valveLzmaData.subspan(12, kLzmaPropsSize);

    if (actualSize > kMaxUncompressedSize) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::OutOfMemory,
            QCoreApplication::translate("Lzma1Decoder", "Valve LZMA uncompressed size exceeds safety limit"),
            QStringLiteral("actualSize=%1, limit=%2")
                .arg(actualSize)
                .arg(kMaxUncompressedSize));
    }

    const std::span<const std::byte> payload = valveLzmaData.subspan(kValveHeaderSize);
    const std::span<const std::byte> compressedStream =
        (static_cast<std::size_t>(lzmaSize) <= payload.size())
        ? payload.subspan(0, lzmaSize)
        : payload;

    return decompressRaw(props5, compressedStream, actualSize);
}

} // namespace Domain::Package::Internal
