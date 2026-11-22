#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Core/Result/Result.h"

namespace Domain::Package::Internal {

/**
 * @brief Standalone read-only LZMA1 range decoder for Valve 'LZMA' BSP lumps
 *        and ZIP Compression Method 14 (LZMA) entries.
 */
class Lzma1Decoder {
public:
    static constexpr std::uint32_t kValveLzmaMagic = 0x414D5A4CU; // 'LZMA' little-endian ('L','Z','M','A')

    /// Returns true if @p data begins with the 4-byte Valve 'LZMA' signature.
    static bool hasValveLzmaMagic(std::span<const std::byte> data) noexcept;

    /// Decompresses a raw LZMA1 stream given its 5-byte properties header and known uncompressed size.
    static Core::Result<std::vector<std::byte>> decompressRaw(
        std::span<const std::byte> props5,
        std::span<const std::byte> compressedStream,
        std::size_t uncompressedSize);

    /// Decompresses a Valve BSP lump prefixed with the 17-byte 'LZMA' header.
    static Core::Result<std::vector<std::byte>> decompressValveLump(
        std::span<const std::byte> valveLzmaData);
};

} // namespace Domain::Package::Internal
