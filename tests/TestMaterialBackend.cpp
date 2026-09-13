// TEMPORARY task-scoped test (AGENTS.md section 9.1): Domain layer tests are
// ephemeral and must be deleted once the material-backend task completes.
// Do not let this file or its CMake target become a permanent resident.

#include <QtTest/QTest>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <random>
#include <span>
#include <vector>

#include <vtfpp/VTF.h>

#include "Core/Async/CancellationToken.h"
#include "Core/Error/Error.h"
#include "Core/Path/FilesystemPath.h"
#include "Domain/Material/TextureImage.h"
#include "Domain/Material/TextureIO.h"
#include "Domain/Material/TgaCodec.h"
#include "Domain/Material/VtfCodec.h"
#include "Domain/Material/TextureProcess/TextureBlur.h"
#include "Domain/Material/TextureProcess/TexturePresets.h"
#include "Domain/Material/TextureProcess/HeightGenerator.h"
#include "Domain/Material/TextureProcess/NormalGenerator.h"
#include "Domain/Material/TextureProcess/AoGenerator.h"
#include "Domain/Material/TextureProcess/MetallicGenerator.h"
#include "Domain/Material/TextureProcess/SmoothnessGenerator.h"
#include "Domain/Material/TextureProcess/ChannelPacker.h"
#include "Domain/Material/TextureProcess/DiffuseEditor.h"

using namespace Domain::Material;
using Domain::Material::TextureProcess::AoGenerator;
using Domain::Material::TextureProcess::AoParams;
using Domain::Material::TextureProcess::ChannelPacker;
using Domain::Material::TextureProcess::ChannelPackParams;
using Domain::Material::TextureProcess::DiffuseEditParams;
using Domain::Material::TextureProcess::DiffuseEditor;
using Domain::Material::TextureProcess::HeightColorSample;
using Domain::Material::TextureProcess::HeightFromNormalParams;
using Domain::Material::TextureProcess::HeightGenerator;
using Domain::Material::TextureProcess::HeightParams;
using Domain::Material::TextureProcess::MetallicGenerator;
using Domain::Material::TextureProcess::MetallicParams;
using Domain::Material::TextureProcess::NormalGenerator;
using Domain::Material::TextureProcess::NormalParams;
using Domain::Material::TextureProcess::PackSource;
using Domain::Material::TextureProcess::SmoothnessGenerator;
using Domain::Material::TextureProcess::SmoothnessParams;
using Domain::Material::TextureProcess::TextureBlur;

namespace {

Core::Path::FilesystemPath pathOf(const QString& string)
{
    return Core::Path::FilesystemPath(string);
}

TextureImage flatImage(int width, int height, int channels, float value)
{
    TextureImage image(width, height, channels);
    image.fill(value);
    return image;
}

/**
 * @brief Builds a 3x2 uncompressed 24-bit TGA stored bottom-up (the on-disk
 *        row order is the reverse of the image's top-down row order).
 */
QByteArray buildTga24BitBottomUp()
{
    QByteArray data;
    data.append(char(0)); // id length
    data.append(char(0)); // color map type
    data.append(char(2)); // uncompressed true-color
    data.append(QByteArray(5, '\0')); // color map spec
    data.append(QByteArray(4, '\0')); // x/y origin
    data.append(char(3)); // width low byte
    data.append(char(0)); // width high byte
    data.append(char(2)); // height low byte
    data.append(char(0)); // height high byte
    data.append(char(24)); // bits per pixel
    data.append(char(0)); // descriptor: bottom-up, no alpha
    // BGR rows, bottom image row first.
    const unsigned char bottomRow[9] = {10, 11, 12, 13, 14, 15, 16, 17, 18};
    const unsigned char topRow[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    data.append(reinterpret_cast<const char*>(bottomRow), 9);
    data.append(reinterpret_cast<const char*>(topRow), 9);
    return data;
}

/**
 * @brief Builds a 2x2 uncompressed 32-bit TGA stored top-down with alpha.
 */
QByteArray buildTga32BitTopDown()
{
    QByteArray data;
    data.append(char(0)); // id length
    data.append(char(0)); // color map type
    data.append(char(2)); // uncompressed true-color
    data.append(QByteArray(5, '\0')); // color map spec
    data.append(QByteArray(4, '\0')); // x/y origin
    data.append(char(2)); // width low byte
    data.append(char(0)); // width high byte
    data.append(char(2)); // height low byte
    data.append(char(0)); // height high byte
    data.append(char(32)); // bits per pixel
    data.append(char(0x28)); // descriptor: top-down (0x20), 8 alpha bits (0x08)
    // BGRA rows, top image row first.
    const unsigned char pixels[16] = {
        10, 20, 30, 40, 50, 60, 70, 80,
        90, 100, 110, 120, 130, 140, 150, 160,
    };
    data.append(reinterpret_cast<const char*>(pixels), 16);
    return data;
}

} // namespace

class TestMaterialBackend : public QObject {
    Q_OBJECT

private slots:
    void textureImagePlanarLayoutAndWrappedSampling();
    void tgaReadHandlesBottomUpAndAlpha();
    void pngGrayscaleRoundtrip();
    void textureIoWriteRestrictedToPng();
    void vtfReadRoundtripsUncompressedAndCompressed();
    void vtfReadRejectsGarbage();
    void blurPreservesConstant();
    void blurImpulsePreservesEnergy();
    void heightFlatDiffuseYieldsMid();
    void heightRejectsInvalidInput();
    void heightCancellationWorks();
    void normalFlatHeightYieldsUpZ();
    void normalRampTiltsMinusX();
    void heightColorSampleIsolateMask();
    void heightFromNormalFlatYieldsMid();
    void heightFromNormalAppliesFinalStage();
    void parameterDefaultsMatchReference();
    void aoFlatInputsYieldUnoccluded();
    void aoPitProducesOcclusion();
    void aoZeroNormalsHandledLikeReference();
    void presetsMatchReferenceValues();
    void metallicUniformMatchesPick();
    void metallicHighPassAddsDetail();
    void smoothnessBaseAndMetalGate();
    void packAssignsChannels();
    void packRejectsMismatchedSizes();
    void diffuseEditDefaultsNearPassthrough();
    void diffuseEditRemovesLightingGradient();
    void endToEndPipelineWithEdit();
    void endToEndVisualPipeline();

private:
    static QString outputDirectory();
};

QString TestMaterialBackend::outputDirectory()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/cs2material_backend");
    QDir().mkpath(dir);
    return dir;
}

void TestMaterialBackend::textureImagePlanarLayoutAndWrappedSampling()
{
    TextureImage image(4, 3, 2);
    QVERIFY(image.isValid());
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            image.at(0, x, y) = static_cast<float>(y * 4 + x);
            image.at(1, x, y) = 100.0f + static_cast<float>(y * 4 + x);
        }
    }
    // Planes must be independent.
    QCOMPARE(image.at(0, 1, 1), 5.0f);
    QCOMPARE(image.at(1, 1, 1), 105.0f);

    // Exact texel centers sample identically.
    QCOMPARE(image.sampleBilinearWrapped(0, 1.0f, 1.0f), 5.0f);
    // Wrap: texel x = -1 lands on x = 3 (row y=1 -> 1*4+3 = 7).
    QCOMPARE(image.sampleBilinearWrapped(0, -1.0f, 1.0f), 7.0f);
    // Wrap: x = 4.5 wraps to 0.5 -> halfway between x=0 and x=1 at y=1.
    QVERIFY(std::abs(image.sampleBilinearWrapped(0, 4.5f, 1.0f) - 4.5f) < 1e-5f);

    TextureImage invalid(0, 0, 5);
    QVERIFY(!invalid.isValid());
    QVERIFY(!TextureImage::isChannelCountValid(0));
    QVERIFY(!TextureImage::isChannelCountValid(5));
}

void TestMaterialBackend::tgaReadHandlesBottomUpAndAlpha()
{
    // 24-bit bottom-up source: read must flip rows and expand BGR to RGBA.
    const QString bottomUpPath = outputDirectory() + QStringLiteral("/tga_bottomup.tga");
    {
        QFile file(bottomUpPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(buildTga24BitBottomUp());
    }
    auto loaded = TgaCodec::read(pathOf(bottomUpPath));
    QVERIFY2(loaded.isSuccess(), qPrintable(loaded.message()));
    const QImage image = loaded.value().convertToFormat(QImage::Format_RGBA8888);
    QCOMPARE(image.width(), 3);
    QCOMPARE(image.height(), 2);
    QCOMPARE(image.pixel(0, 0), qRgb(3, 2, 1));
    QCOMPARE(image.pixel(2, 0), qRgb(9, 8, 7));
    QCOMPARE(image.pixel(0, 1), qRgb(12, 11, 10));
    QCOMPARE(image.pixel(2, 1), qRgb(18, 17, 16));

    // 32-bit top-down source with alpha must survive verbatim (BGR -> RGB).
    const QString topDownPath = outputDirectory() + QStringLiteral("/tga_topdown.tga");
    {
        QFile file(topDownPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(buildTga32BitTopDown());
    }
    auto alphaLoaded = TgaCodec::read(pathOf(topDownPath));
    QVERIFY2(alphaLoaded.isSuccess(), qPrintable(alphaLoaded.message()));
    const QImage alphaImage = alphaLoaded.value().convertToFormat(QImage::Format_RGBA8888);
    QCOMPARE(alphaImage.pixel(0, 0), qRgba(30, 20, 10, 40));
    QCOMPARE(alphaImage.pixel(1, 1), qRgba(150, 140, 130, 160));
}

void TestMaterialBackend::pngGrayscaleRoundtrip()
{
    TextureImage gray(9, 4, 1);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 9; ++x) {
            gray.at(0, x, y) = static_cast<float>(x) / 9.0f;
        }
    }

    const QString path = outputDirectory() + QStringLiteral("/png_gray_roundtrip.png");
    auto written = TextureIO::writeTexture(pathOf(path), gray, false);
    QVERIFY2(written.isSuccess(), qPrintable(written.message()));

    auto loaded = TextureIO::loadTexture(pathOf(path), false);
    QVERIFY2(loaded.isSuccess(), qPrintable(loaded.message()));
    const TextureImage& restored = loaded.value();
    QCOMPARE(restored.width(), 9);
    QCOMPARE(restored.height(), 4);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 9; ++x) {
            // 8-bit quantization error bound.
            QVERIFY(std::abs(restored.at(0, x, y) - gray.at(0, x, y)) < 1.0f / 255.0f);
        }
    }
}

void TestMaterialBackend::textureIoWriteRestrictedToPng()
{
    QVERIFY(TextureIO::isSupportedWriteExtension(QStringLiteral("png")));
    QVERIFY(!TextureIO::isSupportedWriteExtension(QStringLiteral("tga")));
    QVERIFY(!TextureIO::isSupportedWriteExtension(QStringLiteral("jpg")));
    QVERIFY(!TextureIO::isSupportedWriteExtension(QStringLiteral("vtf")));

    const TextureImage gray = flatImage(4, 4, 1, 0.5f);
    auto rejectedTga = TextureIO::writeTexture(
        pathOf(outputDirectory() + QStringLiteral("/rejected.tga")), gray, false);
    QVERIFY(rejectedTga.isFailure());
    QCOMPARE(rejectedTga.error().code(), Core::Error::ErrorCode::NotSupported);
    auto rejectedJpg = TextureIO::writeTexture(
        pathOf(outputDirectory() + QStringLiteral("/rejected.jpg")), gray, false);
    QVERIFY(rejectedJpg.isFailure());

    // The load side stays wide: TGA/VTF extensions must remain accepted.
    QVERIFY(TextureIO::isSupportedLoadExtension(QStringLiteral("tga")));
    QVERIFY(TextureIO::isSupportedLoadExtension(QStringLiteral("vtf")));
    QVERIFY(TextureIO::isSupportedLoadExtension(QStringLiteral("png")));
}

void TestMaterialBackend::vtfReadRoundtripsUncompressedAndCompressed()
{
    const int size = 32;
    std::vector<std::byte> rgba(static_cast<std::size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            auto* pixel = rgba.data() + (static_cast<std::size_t>(y) * size + x) * 4;
            pixel[0] = std::byte{static_cast<unsigned char>(x * 8)};       // R
            pixel[1] = std::byte{static_cast<unsigned char>(y * 8)};       // G
            pixel[2] = std::byte{static_cast<unsigned char>((x + y) * 4)}; // B
            pixel[3] = std::byte{255};                                     // A
        }
    }
    const std::span<const std::byte> sourcePixels{rgba.data(), rgba.size()};

    // Uncompressed RGBA8888 storage must round-trip exactly (raw load path).
    // The output format must be explicit: vtfpp resolves CreationOptions' empty
    // FORMAT_DEFAULT to STRATA_BC7 on version 7+, which is heavily lossy.
    const QString rgbaPath = outputDirectory() + QStringLiteral("/fixture_rgba.vtf");
    vtfpp::VTF::CreationOptions rgbaOptions;
    rgbaOptions.outputFormat = vtfpp::ImageFormat::RGBA8888;
    QVERIFY(vtfpp::VTF::create(sourcePixels, vtfpp::ImageFormat::RGBA8888, size, size,
        std::filesystem::path(rgbaPath.toStdWString()), rgbaOptions));

    auto rgbaLoaded = TextureIO::loadTexture(pathOf(rgbaPath), false);
    QVERIFY2(rgbaLoaded.isSuccess(), qPrintable(rgbaLoaded.message()));
    const TextureImage& rgbaImage = rgbaLoaded.value();
    QCOMPARE(rgbaImage.width(), size);
    QCOMPARE(rgbaImage.height(), size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            QCOMPARE(rgbaImage.at(0, x, y), static_cast<float>(x * 8) / 255.0f);
            QCOMPARE(rgbaImage.at(1, x, y), static_cast<float>(y * 8) / 255.0f);
            QCOMPARE(rgbaImage.at(2, x, y), static_cast<float>((x + y) * 4) / 255.0f);
            QCOMPARE(rgbaImage.at(3, x, y), 1.0f);
        }
    }

    // Compressed storage: hand-build uniform DXT5 blocks (alpha endpoints
    // 200/200, white color) so the fixture exercises vtfpp's bcdec decompress
    // path without its disabled Compressonator compressor. Mip/thumbnail
    // generation is disabled because it would re-enter the missing compressor.
    const QString dxtPath = outputDirectory() + QStringLiteral("/fixture_dxt5.vtf");
    std::vector<std::byte> dxt5(8 * 8 * 16); // 8x8 blocks of 16 bytes for 32x32
    for (auto& block : dxt5) {
        block = std::byte{0};
    }
    for (std::size_t offset = 0; offset < dxt5.size(); offset += 16) {
        dxt5[offset + 0] = std::byte{200}; // alpha endpoint a0
        dxt5[offset + 1] = std::byte{200}; // alpha endpoint a1
        // alpha indices all zero -> alpha decodes to a0 (200) everywhere
        dxt5[offset + 8] = std::byte{0xFF}; // color c0 = RGB565 white
        dxt5[offset + 9] = std::byte{0xFF};
        dxt5[offset + 10] = std::byte{0x00}; // color c1 = black (c0 > c1)
        dxt5[offset + 11] = std::byte{0x00};
        // color indices all zero -> color decodes to c0 (white) everywhere
    }
    vtfpp::VTF::CreationOptions dxtOptions;
    dxtOptions.outputFormat = vtfpp::ImageFormat::DXT5;
    dxtOptions.computeMips = false;
    dxtOptions.computeThumbnail = false;
    dxtOptions.computeTransparencyFlags = false;
    dxtOptions.computeReflectivity = false;
    QVERIFY(vtfpp::VTF::create(std::span<const std::byte>{dxt5.data(), dxt5.size()},
        vtfpp::ImageFormat::DXT5, size, size,
        std::filesystem::path(dxtPath.toStdWString()), dxtOptions));

    auto dxtLoaded = TextureIO::loadTexture(pathOf(dxtPath), false);
    QVERIFY2(dxtLoaded.isSuccess(), qPrintable(dxtLoaded.message()));
    const TextureImage& dxtImage = dxtLoaded.value();
    QCOMPARE(dxtImage.width(), size);
    QCOMPARE(dxtImage.height(), size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            QCOMPARE(dxtImage.at(0, x, y), 1.0f);              // white
            QCOMPARE(dxtImage.at(1, x, y), 1.0f);
            QCOMPARE(dxtImage.at(2, x, y), 1.0f);
            QCOMPARE(dxtImage.at(3, x, y), 200.0f / 255.0f);   // alpha endpoint
        }
    }
}

void TestMaterialBackend::vtfReadRejectsGarbage()
{
    QVERIFY(VtfCodec::isVtfExtension(QStringLiteral("vtf")));
    QVERIFY(!VtfCodec::isVtfExtension(QStringLiteral("png")));

    const QString garbagePath = outputDirectory() + QStringLiteral("/garbage.vtf");
    {
        QFile file(garbagePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(64, '\xAB'));
    }
    auto loaded = TextureIO::loadTexture(pathOf(garbagePath), false);
    QVERIFY(loaded.isFailure());
}

void TestMaterialBackend::blurPreservesConstant()
{
    const TextureImage constant = flatImage(32, 24, 1, 0.3f);
    auto blurred = TextureBlur::blurAxis(constant, TextureBlur::Axis::Horizontal, 4, 2.5f, 1.0f,
        Core::Async::CancellationToken(), {});
    QVERIFY2(blurred.isSuccess(), qPrintable(blurred.message()));
    for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 32; ++x) {
            QVERIFY(std::abs(blurred.value().at(0, x, y) - 0.3f) < 1e-5f);
        }
    }
}

void TestMaterialBackend::blurImpulsePreservesEnergy()
{
    TextureImage impulse(64, 64, 1);
    impulse.fill(0.0f);
    impulse.at(0, 32, 32) = 1.0f;

    auto horizontal = TextureBlur::blurAxis(impulse, TextureBlur::Axis::Horizontal, 4, 1.0f, 1.0f,
        Core::Async::CancellationToken(), {});
    QVERIFY(horizontal.isSuccess());
    auto both = TextureBlur::blurAxis(horizontal.value(), TextureBlur::Axis::Vertical, 4, 1.0f,
        1.0f, Core::Async::CancellationToken(), {});
    QVERIFY2(both.isSuccess(), qPrintable(both.message()));

    // With repeat wrapping and per-position weight normalization, a separable
    // blur preserves the total mass of the image.
    double sum = 0.0;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            sum += both.value().at(0, x, y);
        }
    }
    QVERIFY(std::abs(sum - 1.0) < 1e-3);

    // Center value = (w0 / sumW)^2 with sumW = 4 -> 0.0625.
    QVERIFY(std::abs(both.value().at(0, 32, 32) - 0.0625f) < 1e-4f);
}

void TestMaterialBackend::heightFlatDiffuseYieldsMid()
{
    const TextureImage flat = flatImage(32, 32, 4, 0.5f);
    auto height = HeightGenerator::generateFromDiffuse(flat, HeightParams{},
        Core::Async::CancellationToken(), {});
    QVERIFY2(height.isSuccess(), qPrintable(height.message()));
    QCOMPARE(height.value().channelCount(), 1);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            QVERIFY(std::abs(height.value().at(0, x, y) - 0.5f) < 1e-4f);
        }
    }
}

void TestMaterialBackend::heightRejectsInvalidInput()
{
    TextureImage grayscaleOnly(8, 8, 1);
    grayscaleOnly.fill(0.5f);
    auto rejected = HeightGenerator::generateFromDiffuse(grayscaleOnly, HeightParams{},
        Core::Async::CancellationToken(), {});
    QVERIFY(rejected.isFailure());
}

void TestMaterialBackend::heightCancellationWorks()
{
    Core::Async::CancellationToken token;
    token.cancel();
    const TextureImage flat = flatImage(16, 16, 3, 0.5f);
    auto cancelled = HeightGenerator::generateFromDiffuse(flat, HeightParams{}, token, {});
    QVERIFY(cancelled.isCancelled());
}

void TestMaterialBackend::normalFlatHeightYieldsUpZ()
{
    const TextureImage flat = flatImage(24, 24, 1, 0.5f);
    auto normal = NormalGenerator::generateFromHeight(flat, nullptr, NormalParams{},
        Core::Async::CancellationToken(), {});
    QVERIFY2(normal.isSuccess(), qPrintable(normal.message()));
    QCOMPARE(normal.value().channelCount(), 3);
    for (int y = 1; y < 23; ++y) {
        for (int x = 1; x < 23; ++x) {
            QVERIFY(std::abs(normal.value().at(0, x, y) - 0.5f) < 1e-4f);
            QVERIFY(std::abs(normal.value().at(1, x, y) - 0.5f) < 1e-4f);
            QVERIFY(std::abs(normal.value().at(2, x, y) - 1.0f) < 1e-4f);
        }
    }
}

void TestMaterialBackend::normalRampTiltsMinusX()
{
    TextureImage ramp(64, 64, 1);
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            ramp.at(0, x, y) = static_cast<float>(x) / 63.0f;
        }
    }
    auto normal = NormalGenerator::generateFromHeight(ramp, nullptr, NormalParams{},
        Core::Async::CancellationToken(), {});
    QVERIFY2(normal.isSuccess(), qPrintable(normal.message()));

    // Height rising to the right must tilt normals toward -X (encoded < 0.5).
    const float centerX = normal.value().at(0, 32, 32);
    const float centerY = normal.value().at(1, 32, 32);
    QVERIFY(centerX < 0.35f);
    QVERIFY(std::abs(centerY - 0.5f) < 0.01f);
}

void TestMaterialBackend::heightColorSampleIsolateMask()
{
    // Left half pure red, right half pure blue (linear values).
    TextureImage color(32, 16, 4);
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 32; ++x) {
            const bool red = x < 16;
            color.at(0, x, y) = red ? 1.0f : 0.0f;
            color.at(1, x, y) = 0.0f;
            color.at(2, x, y) = red ? 0.0f : 1.0f;
            color.at(3, x, y) = 1.0f;
        }
    }

    HeightParams params;
    params.sample1.enabled = true;
    params.sample1.isolate = true;
    params.sample1.colorR = 1.0f;
    params.sample1.colorG = 0.0f;
    params.sample1.colorB = 0.0f;

    auto height = HeightGenerator::generateFromDiffuse(color, params,
        Core::Async::CancellationToken(), {});
    QVERIFY2(height.isSuccess(), qPrintable(height.message()));

    // The generator output passes through the frequency combine, so exact
    // step-1 mask values cannot be asserted directly. Isolate mode must still
    // separate the hues: the red half ends clearly above the blue half.
    const float redSide = height.value().at(0, 4, 8);
    const float blueSide = height.value().at(0, 27, 8);
    QVERIFY2(redSide > blueSide + 0.05f,
        qPrintable(QStringLiteral("red=%1 blue=%2").arg(redSide).arg(blueSide)));
}

void TestMaterialBackend::heightFromNormalFlatYieldsMid()
{
    TextureImage normal(32, 32, 3);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            normal.at(0, x, y) = 0.5f;
            normal.at(1, x, y) = 0.5f;
            normal.at(2, x, y) = 1.0f;
        }
    }
    HeightFromNormalParams params;
    params.iterations = 4; // keep the test fast; flat result is iteration-independent
    auto height = HeightGenerator::generateFromNormal(normal, params,
        Core::Async::CancellationToken(), {});
    QVERIFY2(height.isSuccess(), qPrintable(height.message()));
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            QVERIFY(std::abs(height.value().at(0, x, y) - 0.5f) < 1e-4f);
        }
    }
}

void TestMaterialBackend::heightFromNormalAppliesFinalStage()
{
    // Sawtooth tilt gradients (period 8, amplitude 0.5): the radial taps at
    // offsets 1..4 cover half a period, so the accumulation is non-constant
    // and the final stage has something to act on. (A symmetric high-frequency
    // pattern would be averaged back to the 0.5 fixed point.)
    TextureImage normal(32, 32, 3);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            normal.at(0, x, y) = 0.25f + 0.5f * static_cast<float>(x % 8) / 8.0f;
            normal.at(1, x, y) = 0.25f + 0.5f * static_cast<float>(y % 8) / 8.0f;
            normal.at(2, x, y) = 1.0f;
        }
    }

    // Identity final stage isolates the raw accumulated result. A small spread
    // keeps the radial sweep local, so the alternating pattern survives the
    // accumulation (the default spread would wrap-sample the whole image and
    // average it back to the 0.5 fixed point).
    HeightFromNormalParams rawParams;
    rawParams.iterations = 4;
    rawParams.spread = 4.0f;
    rawParams.finalContrast = 1.0f;
    auto raw = HeightGenerator::generateFromNormal(normal, rawParams,
        Core::Async::CancellationToken(), {});
    QVERIFY2(raw.isSuccess(), qPrintable(raw.message()));

    // Reference default finalContrast of 1.5 must be applied after the
    // accumulation (fragCombineHeight, HeightFromNormal branch).
    HeightFromNormalParams contrastParams; // finalContrast = 1.5
    contrastParams.iterations = 4;
    contrastParams.spread = 4.0f;
    auto contrasted = HeightGenerator::generateFromNormal(normal, contrastParams,
        Core::Async::CancellationToken(), {});
    QVERIFY2(contrasted.isSuccess(), qPrintable(contrasted.message()));

    bool differs = false;
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const float base = raw.value().at(0, x, y);
            const float expected = std::clamp((base - 0.5f) * 1.5f + 0.5f, 0.0f, 1.0f);
            // finalGain 0 -> realGain 1 -> identity gain curve.
            QVERIFY(std::abs(contrasted.value().at(0, x, y) - expected) < 1e-4f);
            differs = differs || std::abs(base - expected) > 1e-3f;
        }
    }
    QVERIFY(differs); // the reference default contrast of 1.5 is non-trivial

    // Negative UI gain inverts the curve via abs(1 / (g - 1)) exactly like
    // the reference orchestration (HeightFromDiffuseGui).
    HeightFromNormalParams gainParams;
    gainParams.iterations = 4;
    gainParams.spread = 4.0f;
    gainParams.finalGain = -0.5f; // realGain = abs(1 / -1.5) = 2/3
    auto gained = HeightGenerator::generateFromNormal(normal, gainParams,
        Core::Async::CancellationToken(), {});
    QVERIFY2(gained.isSuccess(), qPrintable(gained.message()));
    const float realGain = std::abs(1.0f / (-0.5f - 1.0f));
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const float base = std::clamp(
                (raw.value().at(0, x, y) - 0.5f) * 1.5f + 0.5f, 0.0f, 1.0f);
            float expected = 0.0f;
            if (base > 0.5f) {
                expected = std::pow(std::clamp(base * 2.0f - 1.0f, 0.0f, 1.0f), realGain) * 0.5f
                    + 0.5f;
            } else {
                expected = 1.0f
                    - (std::pow(std::clamp((1.0f - base) * 2.0f - 1.0f, 0.0f, 1.0f), realGain)
                            * 0.5f
                        + 0.5f);
            }
            QVERIFY(std::abs(gained.value().at(0, x, y) - expected) < 1e-4f);
        }
    }
}

void TestMaterialBackend::parameterDefaultsMatchReference()
{
    // Reference ctor defaults: Sample1Height/Sample1Smoothness = 0.5,
    // Sample2Height/Sample2Smoothness = 0.3 (HeightFromDiffuseSettings.cs,
    // SmoothnessSettings.cs).
    HeightParams heightParams;
    QCOMPARE(heightParams.sample1.height, 0.5f);
    QCOMPARE(heightParams.sample2.height, 0.3f);
    QCOMPARE(heightParams.sample1.hueWeight, 1.0f);
    QCOMPARE(heightParams.sample1.satWeight, 0.5f);
    QCOMPARE(heightParams.sample1.lumWeight, 0.2f);

    SmoothnessParams smoothnessParams;
    QCOMPARE(smoothnessParams.sample1.smoothness, 0.5f);
    QCOMPARE(smoothnessParams.sample2.smoothness, 0.3f);
    QCOMPARE(smoothnessParams.baseSmoothness, 0.1f);
    QCOMPARE(smoothnessParams.metalSmoothness, 0.7f);
    QCOMPARE(smoothnessParams.highPassOverlay, 3.0f);

    // Height-from-normal carries the reference final-stage defaults
    // (FinalContrast 1.5 is non-trivial and must not regress to identity).
    HeightFromNormalParams heightFromNormalParams;
    QCOMPARE(heightFromNormalParams.finalContrast, 1.5f);
    QCOMPARE(heightFromNormalParams.finalBias, 0.0f);
    QCOMPARE(heightFromNormalParams.finalGain, 0.0f);
}

void TestMaterialBackend::aoFlatInputsYieldUnoccluded()
{
    TextureImage normal(32, 32, 3);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            normal.at(0, x, y) = 0.5f;
            normal.at(1, x, y) = 0.5f;
            normal.at(2, x, y) = 1.0f;
        }
    }
    TextureImage height(32, 32, 1);
    height.fill(0.5f);

    AoParams params;
    params.iterations = 4;
    auto ao = AoGenerator::generate(normal, &height, params, Core::Async::CancellationToken(), {});
    QVERIFY2(ao.isSuccess(), qPrintable(ao.message()));

    // Flat normal + flat height -> no occlusion anywhere: depth term is 1.
    // With the default normal/depth blend of 1.0 the result must be white.
    for (int y = 2; y < 30; ++y) {
        for (int x = 2; x < 30; ++x) {
            QVERIFY(std::abs(ao.value().at(0, x, y) - 1.0f) < 1e-3f);
        }
    }
}

void TestMaterialBackend::metallicUniformMatchesPick()
{
    // Uniform diffuse exactly matching the picked metal color: the high-pass
    // overlay is zero, and hue/sat/lum differences are all zero, so
    // mask = (1*a + 1*b + 1*c)/(a+b+c) = 1.0 -> fully metallic.
    const TextureImage flat = flatImage(16, 16, 3, 0.5f);
    MetallicParams params;
    params.metalColorR = 0.5f;
    params.metalColorG = 0.5f;
    params.metalColorB = 0.5f;

    auto metallic = MetallicGenerator::generate(flat, params, Core::Async::CancellationToken(), {});
    QVERIFY2(metallic.isSuccess(), qPrintable(metallic.message()));
    QVERIFY(std::abs(metallic.value().at(0, 8, 8) - 1.0f) < 0.01f);
}

void TestMaterialBackend::metallicHighPassAddsDetail()
{
    // Uniform mid-grey with one bright stripe: with highPassOverlay > 0 the
    // stripe must end more metallic than the flat ground.
    TextureImage color(32, 32, 3);
    color.fill(0.5f);
    for (int y = 12; y < 20; ++y) {
        for (int x = 0; x < 32; ++x) {
            color.at(0, x, y) = 0.9f;
            color.at(1, x, y) = 0.9f;
            color.at(2, x, y) = 0.9f;
        }
    }
    MetallicParams params;
    params.metalColorR = 0.5f;
    params.metalColorG = 0.5f;
    params.metalColorB = 0.5f;

    auto metallic = MetallicGenerator::generate(color, params, Core::Async::CancellationToken(), {});
    QVERIFY2(metallic.isSuccess(), qPrintable(metallic.message()));
    QVERIFY(metallic.value().at(0, 16, 16) > metallic.value().at(0, 16, 4) + 0.05f);
}

void TestMaterialBackend::smoothnessBaseAndMetalGate()
{
    const TextureImage flat = flatImage(16, 16, 3, 0.5f);

    // No samples, no metallic map: pure base smoothness.
    SmoothnessParams baseParams;
    auto base = SmoothnessGenerator::generate(flat, nullptr, baseParams,
        Core::Async::CancellationToken(), {});
    QVERIFY2(base.isSuccess(), qPrintable(base.message()));
    QVERIFY(std::abs(base.value().at(0, 8, 8) - baseParams.baseSmoothness) < 0.01f);

    // A white metallic map forces the metal smoothness (gate applied last).
    TextureImage metalMask = flatImage(16, 16, 1, 1.0f);
    auto gated = SmoothnessGenerator::generate(flat, &metalMask, SmoothnessParams{},
        Core::Async::CancellationToken(), {});
    QVERIFY2(gated.isSuccess(), qPrintable(gated.message()));
    QVERIFY(std::abs(gated.value().at(0, 8, 8) - SmoothnessParams{}.metalSmoothness) < 0.01f);
}

void TestMaterialBackend::packAssignsChannels()
{
    TextureImage metal(8, 8, 1);
    metal.fill(0.25f);
    TextureImage rough(8, 8, 1);
    rough.fill(0.5f);
    TextureImage ao(8, 8, 1);
    ao.fill(0.75f);

    ChannelPackParams params;
    params.red.image = &metal;
    params.green.image = &rough;
    params.blue.image = &ao;
    // alpha unassigned -> 3-channel output.

    auto packed = ChannelPacker::pack(params, Core::Async::CancellationToken(), {});
    QVERIFY2(packed.isSuccess(), qPrintable(packed.message()));
    QCOMPARE(packed.value().channelCount(), 3);
    QVERIFY(std::abs(packed.value().at(0, 3, 3) - 0.25f) < 1e-5f);
    QVERIFY(std::abs(packed.value().at(1, 3, 3) - 0.5f) < 1e-5f);
    QVERIFY(std::abs(packed.value().at(2, 3, 3) - 0.75f) < 1e-5f);

    // With alpha assigned the output becomes 4-channel.
    TextureImage alphaSrc(8, 8, 1);
    alphaSrc.fill(0.1f);
    ChannelPackParams rgbaParams;
    rgbaParams.red.image = &metal;
    rgbaParams.green.image = &rough;
    rgbaParams.blue.image = &ao;
    rgbaParams.alpha.image = &alphaSrc;
    rgbaParams.defaultAlpha = 0.9f;
    auto packedRgba = ChannelPacker::pack(rgbaParams, Core::Async::CancellationToken(), {});
    QVERIFY2(packedRgba.isSuccess(), qPrintable(packedRgba.message()));
    QCOMPARE(packedRgba.value().channelCount(), 4);
    QVERIFY(std::abs(packedRgba.value().at(3, 3, 3) - 0.1f) < 1e-5f);

    // Luminance extraction from a color source.
    TextureImage color(8, 8, 4);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            color.at(0, x, y) = 1.0f;
            color.at(1, x, y) = 0.0f;
            color.at(2, x, y) = 0.0f;
            color.at(3, x, y) = 1.0f;
        }
    }
    ChannelPackParams lumParams;
    lumParams.red.image = &color;
    lumParams.red.source = PackSource::Luminance;
    auto packedLum = ChannelPacker::pack(lumParams, Core::Async::CancellationToken(), {});
    QVERIFY2(packedLum.isSuccess(), qPrintable(packedLum.message()));
    QVERIFY(std::abs(packedLum.value().at(0, 3, 3) - 0.3f) < 1e-5f);
}

void TestMaterialBackend::packRejectsMismatchedSizes()
{
    TextureImage a(8, 8, 1);
    a.fill(1.0f);
    TextureImage b(4, 4, 1);
    b.fill(1.0f);

    ChannelPackParams params;
    params.red.image = &a;
    params.green.image = &b;
    auto rejected = ChannelPacker::pack(params, Core::Async::CancellationToken(), {});
    QVERIFY(rejected.isFailure());
}

void TestMaterialBackend::aoPitProducesOcclusion()
{
    // Flat normal with a deep hemispherical pit in the height map.
    const int size = 48;
    TextureImage normal(size, size, 3);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            normal.at(0, x, y) = 0.5f;
            normal.at(1, x, y) = 0.5f;
            normal.at(2, x, y) = 1.0f;
        }
    }
    TextureImage height(size, size, 1);
    height.fill(0.5f);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = static_cast<float>(x) - size / 2.0f;
            const float dy = static_cast<float>(y) - size / 2.0f;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d < 8.0f) {
                height.at(0, x, y) = 0.5f - 0.4f * (1.0f - d / 8.0f);
            }
        }
    }

    AoParams params;
    params.iterations = 8;
    params.samplesPerIteration = 24;
    auto ao = AoGenerator::generate(normal, &height, params, Core::Async::CancellationToken(), {});
    QVERIFY2(ao.isSuccess(), qPrintable(ao.message()));

    // The pit must occlude: darker inside than on the far flat rim.
    const float center = ao.value().at(0, size / 2, size / 2);
    const float rim = ao.value().at(0, 4, 4);
    QVERIFY2(center < rim - 0.05f,
        qPrintable(QStringLiteral("center=%1 rim=%2").arg(center).arg(rim)));
}

void TestMaterialBackend::aoZeroNormalsHandledLikeReference()
{
    // Encoded 0.5 decodes to a zero-length normal: the reference keeps such a
    // sample's importance in the denominator while it contributes 0 to the
    // numerator (no per-sample normalization, no skip). The all-zero region
    // must therefore settle at the shaped zero response
    // sqrt(pow(1,5) * pow(0.5,0.2)) = 0.5^0.1.
    TextureImage normal(16, 16, 3);
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const bool up = x >= 8 && y >= 8;
            normal.at(0, x, y) = 0.5f;
            normal.at(1, x, y) = up ? 1.0f : 0.5f;
            normal.at(2, x, y) = 0.5f;
        }
    }

    AoParams params;
    params.iterations = 2;
    params.samplesPerIteration = 4;
    params.spread = 2.0f; // keep every sample inside its own quadrant
    auto ao = AoGenerator::generate(normal, nullptr, params, Core::Async::CancellationToken(), {});
    QVERIFY2(ao.isSuccess(), qPrintable(ao.message()));

    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const float value = ao.value().at(0, x, y);
            QVERIFY(std::isfinite(value));
            QVERIFY(value >= 0.0f && value <= 1.0f);
        }
    }
    const float shapedZero = std::pow(0.5f, 0.1f);
    // Interior pixels of the zero quadrant only ever sample zero normals.
    QVERIFY2(std::abs(ao.value().at(0, 3, 3) - shapedZero) < 1e-3f,
        qPrintable(QStringLiteral("zeroRegion=%1").arg(ao.value().at(0, 3, 3))));
    // The +Y quadrant must respond differently (tilted normals sweep).
    QVERIFY2(std::abs(ao.value().at(0, 11, 11) - shapedZero) > 1e-3f,
        qPrintable(QStringLiteral("upRegion=%1").arg(ao.value().at(0, 11, 11))));
}

void TestMaterialBackend::presetsMatchReferenceValues()
{
    using namespace Domain::Material::TextureProcess;
    QCOMPARE(kHeightWeightPresets.size(), 3);
    QCOMPARE(kHeightContrastPresets.size(), 3);
    QCOMPARE(kNormalWeightPresets.size(), 4);

    const auto& displace = kHeightWeightPresets[2];
    QVERIFY(displace.id == QByteArray("Displace"));
    QCOMPARE(displace.weights[0], 0.02f);
    QCOMPARE(displace.weights[6], 1.0f);

    const auto& cracked = kHeightContrastPresets[1];
    QCOMPARE(cracked.contrasts[4], -0.2f);
    QCOMPARE(cracked.contrasts[6], -4.0f);

    const auto& funky = kHeightContrastPresets[2];
    QCOMPARE(funky.contrasts[0], -3.0f);
    QCOMPARE(funky.contrasts[5], 2.5f);

    const auto& crisp = kNormalWeightPresets[2];
    QVERIFY(crisp.id == QByteArray("Crisp"));
    QCOMPARE(crisp.weights[0], 1.0f);
    QCOMPARE(crisp.weights[6], 0.1f);

    const auto& mids = kNormalWeightPresets[3];
    QCOMPARE(mids.weights[3], 1.0f);
    QCOMPARE(mids.weights[0], 0.15f);
}

void TestMaterialBackend::diffuseEditDefaultsNearPassthrough()
{
    // With defaults all removal strengths are zero; a uniform color image
    // must come out (nearly) unchanged.
    const TextureImage flat = flatImage(16, 16, 3, 0.5f);
    auto edited = DiffuseEditor::edit(flat, DiffuseEditParams{}, Core::Async::CancellationToken(),
        {});
    QVERIFY2(edited.isSuccess(), qPrintable(edited.message()));
    for (int y = 2; y < 14; ++y) {
        for (int x = 2; x < 14; ++x) {
            QVERIFY(std::abs(edited.value().at(0, x, y) - 0.5f) < 0.05f);
            QVERIFY(std::abs(edited.value().at(1, x, y) - 0.5f) < 0.05f);
            QVERIFY(std::abs(edited.value().at(2, x, y) - 0.5f) < 0.05f);
        }
    }
}

void TestMaterialBackend::diffuseEditRemovesLightingGradient()
{
    // Constant-color ground with a strong horizontal lighting gradient.
    const int size = 64;
    TextureImage color(size, size, 3);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float lighting = 0.3f + 0.6f * static_cast<float>(x) / (size - 1);
            color.at(0, x, y) = 0.4f * lighting;
            color.at(1, x, y) = 0.5f * lighting;
            color.at(2, x, y) = 0.6f * lighting;
        }
    }

    // Luminance spread of the input along a middle row.
    float inputMin = 1.0f;
    float inputMax = 0.0f;
    for (int x = 4; x < size - 4; ++x) {
        const float lum = color.at(0, x, size / 2) * 0.3f + color.at(1, x, size / 2) * 0.5f
            + color.at(2, x, size / 2) * 0.2f;
        inputMin = std::min(inputMin, lum);
        inputMax = std::max(inputMax, lum);
    }

    DiffuseEditParams params;
    params.removeLight = 0.9f;
    params.removeShadow = 0.9f;
    params.keepOriginalColor = 1.0f;
    auto edited = DiffuseEditor::edit(color, params, Core::Async::CancellationToken(), {});
    QVERIFY2(edited.isSuccess(), qPrintable(edited.message()));

    float outputMin = 1.0f;
    float outputMax = 0.0f;
    for (int x = 4; x < size - 4; ++x) {
        const float lum = edited.value().at(0, x, size / 2) * 0.3f
            + edited.value().at(1, x, size / 2) * 0.5f + edited.value().at(2, x, size / 2) * 0.2f;
        outputMin = std::min(outputMin, lum);
        outputMax = std::max(outputMax, lum);
    }
    QVERIFY2((outputMax - outputMin) < (inputMax - inputMin) * 0.5f,
        qPrintable(QStringLiteral("in spread=%1 out spread=%2")
                .arg(inputMax - inputMin)
                .arg(outputMax - outputMin)));
}

void TestMaterialBackend::endToEndPipelineWithEdit()
{
    // The full chain: diffuse -> edit -> height -> normal -> AO -> pack.
    const int size = 128;
    TextureImage color(size, size, 4);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> noise(0.0f, 0.05f);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = static_cast<float>(x) / size - 0.5f;
            const float dy = static_cast<float>(y) / size - 0.5f;
            const float dome = std::max(0.0f, 1.0f - (dx * dx + dy * dy) * 8.0f);
            const float shade = std::clamp(0.35f + dome * 0.45f + noise(rng), 0.0f, 1.0f);
            color.at(0, x, y) = shade * 0.9f;
            color.at(1, x, y) = shade * 0.8f;
            color.at(2, x, y) = shade * 0.7f;
            color.at(3, x, y) = 1.0f;
        }
    }

    Core::Async::CancellationToken token;
    auto edited = DiffuseEditor::edit(color, DiffuseEditParams{}, token, {});
    QVERIFY2(edited.isSuccess(), qPrintable(edited.message()));

    auto height = HeightGenerator::generateFromDiffuse(edited.value(), HeightParams{}, token, {});
    QVERIFY2(height.isSuccess(), qPrintable(height.message()));

    auto normal = NormalGenerator::generateFromHeight(height.value(), &edited.value(),
        NormalParams{}, token, {});
    QVERIFY2(normal.isSuccess(), qPrintable(normal.message()));

    AoParams aoParams;
    aoParams.iterations = 8;
    aoParams.samplesPerIteration = 16;
    auto ao = AoGenerator::generate(normal.value(), &height.value(), aoParams, token, {});
    QVERIFY2(ao.isSuccess(), qPrintable(ao.message()));

    MetallicParams metalParams;
    metalParams.metalColorR = 0.4f;
    metalParams.metalColorG = 0.35f;
    metalParams.metalColorB = 0.3f;
    auto metallic = MetallicGenerator::generate(edited.value(), metalParams, token, {});
    QVERIFY2(metallic.isSuccess(), qPrintable(metallic.message()));

    auto smooth = SmoothnessGenerator::generate(edited.value(), &metallic.value(),
        SmoothnessParams{}, token, {});
    QVERIFY2(smooth.isSuccess(), qPrintable(smooth.message()));

    TextureImage roughness(size, size, 1);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            roughness.at(0, x, y) = 1.0f - smooth.value().at(0, x, y);
        }
    }
    ChannelPackParams packParams;
    packParams.red.image = &metallic.value();
    packParams.green.image = &roughness;
    packParams.blue.image = &ao.value();
    auto packed = ChannelPacker::pack(packParams, token, {});
    QVERIFY2(packed.isSuccess(), qPrintable(packed.message()));

    QCOMPARE(packed.value().channelCount(), 3);
    QCOMPARE(packed.value().width(), size);
}

void TestMaterialBackend::endToEndVisualPipeline()
{
    const QString outDir = outputDirectory();

    QString samplePath = qEnvironmentVariable("MATERIAL_BACKEND_SAMPLE");
    TextureImage color;
    if (!samplePath.isEmpty() && QFileInfo::exists(samplePath)) {
        // Optional downscale for fast visual passes on huge photos.
        bool scaled = false;
        const int maxSide = qEnvironmentVariableIntValue("MATERIAL_BACKEND_MAXSIZE", &scaled);
        QImage loadedImage;
        if (scaled && maxSide > 0) {
            QImageReader reader(samplePath);
            const QSize original = reader.size();
            if (original.isValid()
                && std::max(original.width(), original.height()) > maxSide) {
                QSize target = original;
                if (original.width() >= original.height()) {
                    target.setHeight(std::max(1, original.height() * maxSide / original.width()));
                    target.setWidth(maxSide);
                } else {
                    target.setWidth(std::max(1, original.width() * maxSide / original.height()));
                    target.setHeight(maxSide);
                }
                reader.setScaledSize(target);
            }
            loadedImage = reader.read();
            QVERIFY(!loadedImage.isNull());
            const QString scaledPath = outDir + QStringLiteral("/_scaled_input.png");
            QVERIFY(loadedImage.save(scaledPath, "png"));
            auto loaded = TextureIO::loadTexture(pathOf(scaledPath), true);
            QVERIFY2(loaded.isSuccess(), qPrintable(loaded.message()));
            color = std::move(loaded.value());
        } else {
            auto loaded = TextureIO::loadTexture(pathOf(samplePath), true);
            QVERIFY2(loaded.isSuccess(), qPrintable(loaded.message()));
            color = std::move(loaded.value());
        }
    } else {
        // Procedural fallback: colored ground with domes and dents.
        const int size = 512;
        color = TextureImage(size, size, 4);
        std::mt19937 rng(1234);
        std::uniform_real_distribution<float> noise(0.0f, 0.04f);
        struct Blob { float cx, cy, r, depth; };
        const std::vector<Blob> blobs = {
            {0.25f, 0.30f, 0.18f, 0.35f}, {0.70f, 0.25f, 0.12f, -0.25f},
            {0.60f, 0.70f, 0.20f, 0.30f}, {0.20f, 0.75f, 0.10f, -0.30f},
        };
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const float u = static_cast<float>(x) / size;
                const float v = static_cast<float>(y) / size;
                float h = 0.0f;
                for (const auto& blob : blobs) {
                    const float dx = u - blob.cx;
                    const float dy = v - blob.cy;
                    const float d = std::sqrt(dx * dx + dy * dy);
                    if (d < blob.r) {
                        const float t = d / blob.r;
                        h += blob.depth * (1.0f - t * t);
                    }
                }
                const float shade = std::clamp(0.55f + h * 0.6f + noise(rng), 0.0f, 1.0f);
                color.at(0, x, y) = shade * 0.85f;
                color.at(1, x, y) = shade * 0.75f;
                color.at(2, x, y) = shade * 0.65f;
                color.at(3, x, y) = 1.0f;
            }
        }
    }

    Core::Async::CancellationToken token;

    auto writtenSample = TextureIO::writeTexture(pathOf(outDir + QStringLiteral("/input_color.png")),
        color, true);
    QVERIFY2(writtenSample.isSuccess(), qPrintable(writtenSample.message()));

    // Diffuse edit pass (defaults), then everything downstream consumes it.
    auto edited = DiffuseEditor::edit(color, DiffuseEditParams{}, token, {});
    QVERIFY2(edited.isSuccess(), qPrintable(edited.message()));
    auto writtenEdited = TextureIO::writeTexture(
        pathOf(outDir + QStringLiteral("/edited_color.png")), edited.value(), true);
    QVERIFY2(writtenEdited.isSuccess(), qPrintable(writtenEdited.message()));
    color = std::move(edited.value());

    auto height = HeightGenerator::generateFromDiffuse(color, HeightParams{}, token, {});
    QVERIFY2(height.isSuccess(), qPrintable(height.message()));
    auto writtenHeight = TextureIO::writeTexture(pathOf(outDir + QStringLiteral("/height.png")),
        height.value(), true);
    QVERIFY2(writtenHeight.isSuccess(), qPrintable(writtenHeight.message()));

    auto normal = NormalGenerator::generateFromHeight(height.value(), &color, NormalParams{}, token,
        {});
    QVERIFY2(normal.isSuccess(), qPrintable(normal.message()));
    auto writtenNormal = TextureIO::writeTexture(pathOf(outDir + QStringLiteral("/normal.png")),
        normal.value(), false);
    QVERIFY2(writtenNormal.isSuccess(), qPrintable(writtenNormal.message()));

    // AO at reduced quality for the visual pass (full quality is the default).
    AoParams aoParams;
    aoParams.iterations = 8;
    aoParams.samplesPerIteration = 16;
    auto ao = AoGenerator::generate(normal.value(), &height.value(), aoParams, token, {});
    QVERIFY2(ao.isSuccess(), qPrintable(ao.message()));
    auto writtenAo = TextureIO::writeTexture(pathOf(outDir + QStringLiteral("/ao.png")),
        ao.value(), false);
    QVERIFY2(writtenAo.isSuccess(), qPrintable(writtenAo.message()));

    HeightFromNormalParams hfnParams;
    hfnParams.iterations = 4;
    hfnParams.spread = 32.0f;
    auto heightFromNormal = HeightGenerator::generateFromNormal(normal.value(), hfnParams, token, {});
    QVERIFY2(heightFromNormal.isSuccess(), qPrintable(heightFromNormal.message()));
    auto writtenHfn = TextureIO::writeTexture(
        pathOf(outDir + QStringLiteral("/height_from_normal.png")), heightFromNormal.value(), true);
    QVERIFY2(writtenHfn.isSuccess(), qPrintable(writtenHfn.message()));

    // Metallic/smoothness with the pick color sampled from the input average.
    float avgR = 0.0f;
    float avgG = 0.0f;
    float avgB = 0.0f;
    {
        qsizetype count = 0;
        for (int y = 0; y < color.height(); y += 8) {
            for (int x = 0; x < color.width(); x += 8) {
                avgR += color.at(0, x, y);
                avgG += color.at(1, x, y);
                avgB += color.at(2, x, y);
                ++count;
            }
        }
        avgR /= static_cast<float>(count);
        avgG /= static_cast<float>(count);
        avgB /= static_cast<float>(count);
    }
    MetallicParams metalParams;
    metalParams.metalColorR = avgR;
    metalParams.metalColorG = avgG;
    metalParams.metalColorB = avgB;
    auto metallic = MetallicGenerator::generate(color, metalParams, token, {});
    QVERIFY2(metallic.isSuccess(), qPrintable(metallic.message()));
    auto writtenMetal = TextureIO::writeTexture(
        pathOf(outDir + QStringLiteral("/metallic.png")), metallic.value(), false);
    QVERIFY2(writtenMetal.isSuccess(), qPrintable(writtenMetal.message()));

    SmoothnessParams smoothParams;
    auto smooth = SmoothnessGenerator::generate(color, &metallic.value(), smoothParams, token, {});
    QVERIFY2(smooth.isSuccess(), qPrintable(smooth.message()));
    auto writtenSmooth = TextureIO::writeTexture(
        pathOf(outDir + QStringLiteral("/smoothness.png")), smooth.value(), false);
    QVERIFY2(writtenSmooth.isSuccess(), qPrintable(writtenSmooth.message()));

    // Pseudo-MRAO pack (R=metal, G=1-smoothness as roughness, B=AO).
    TextureImage roughness(height.value().width(), height.value().height(), 1);
    for (int y = 0; y < roughness.height(); ++y) {
        for (int x = 0; x < roughness.width(); ++x) {
            roughness.at(0, x, y) = 1.0f - smooth.value().at(0, x, y);
        }
    }
    ChannelPackParams packParams;
    packParams.red.image = &metallic.value();
    packParams.green.image = &roughness;
    packParams.blue.image = &ao.value();
    auto packed = ChannelPacker::pack(packParams, token, {});
    QVERIFY2(packed.isSuccess(), qPrintable(packed.message()));
    auto writtenPacked = TextureIO::writeTexture(
        pathOf(outDir + QStringLiteral("/packed_mrao.png")), packed.value(), false);
    QVERIFY2(writtenPacked.isSuccess(), qPrintable(writtenPacked.message()));

    qDebug() << "Visual outputs written to" << outDir;
}

QTEST_MAIN(TestMaterialBackend)
#include "TestMaterialBackend.moc"
