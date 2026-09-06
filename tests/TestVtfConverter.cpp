#include <cstring>
#include <filesystem>
#include <vector>

#include <QTest>
#include <QFile>
#include <QTemporaryDir>

#include <vtfpp/VTF.h>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Domain/Material/VtfConverter.h"

using Core::Path::FilesystemPath;
using Domain::Material::VtfConverter;

namespace {

constexpr char kPngMagic[] = "\x89PNG\r\n\x1a\n";

QByteArray readFileBytes(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

} // namespace

class TestVtfConverter : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void convertToImageBufferProducesPng();
    void convertToImageFileWritesFile();
    void missingFileFails();
    void garbageFileFails();
    void convertToMultipleFormats();
    void formatExtensionMapping();

private:
    QTemporaryDir m_dir;
    QString m_vtfPath;
    QString m_garbagePath;
};

void TestVtfConverter::initTestCase() {
    QVERIFY(m_dir.isValid());

    // 4x4 solid red RGBA8888 image encoded into a VTF.
    std::vector<std::byte> pixels(4 * 4 * 4);
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = std::byte{0xFF};
        pixels[i + 1] = std::byte{0x00};
        pixels[i + 2] = std::byte{0x00};
        pixels[i + 3] = std::byte{0xFF};
    }
    m_vtfPath = m_dir.filePath(QStringLiteral("test.vtf"));
    QVERIFY(vtfpp::VTF::create(pixels, vtfpp::ImageFormat::RGBA8888, 4, 4,
        std::filesystem::path{m_vtfPath.toStdWString()}, {}));

    m_garbagePath = m_dir.filePath(QStringLiteral("garbage.vtf"));
    QFile garbage(m_garbagePath);
    QVERIFY(garbage.open(QIODevice::WriteOnly | QIODevice::Truncate));
    garbage.write("this is not a vtf file");
    garbage.close();
}

void TestVtfConverter::convertToImageBufferProducesPng() {
    auto png = VtfConverter::convertToImageBuffer(FilesystemPath(m_vtfPath));
    QVERIFY(png.isSuccess());
    QVERIFY(png.value().size() > 8);
    QCOMPARE(std::memcmp(png.value().data(), kPngMagic, 8), 0);
}

void TestVtfConverter::convertToImageFileWritesFile() {
    const QString destPng = m_dir.filePath(QStringLiteral("out/test.png"));

    auto converted = VtfConverter::convertToImageFile(FilesystemPath(m_vtfPath), FilesystemPath(destPng));
    QVERIFY(converted.isSuccess());

    const QByteArray data = readFileBytes(destPng);
    QVERIFY(data.size() > 8);
    QCOMPARE(std::memcmp(data.constData(), kPngMagic, 8), 0);
}

void TestVtfConverter::convertToMultipleFormats() {
    using Domain::Material::ImageFileFormat;

    // Test TGA conversion
    const QString destTga = m_dir.filePath(QStringLiteral("out/test.tga"));
    auto tgaRes = VtfConverter::convertToImageFile(
        FilesystemPath(m_vtfPath), FilesystemPath(destTga), ImageFileFormat::Tga);
    QVERIFY(tgaRes.isSuccess());
    QVERIFY(QFile::exists(destTga));
    QVERIFY(readFileBytes(destTga).size() > 18); // TGA header is at least 18 bytes

    // Test BMP conversion (BM magic)
    const QString destBmp = m_dir.filePath(QStringLiteral("out/test.bmp"));
    auto bmpRes = VtfConverter::convertToImageFile(
        FilesystemPath(m_vtfPath), FilesystemPath(destBmp), ImageFileFormat::Bmp);
    QVERIFY(bmpRes.isSuccess());
    QVERIFY(QFile::exists(destBmp));
    const auto bmpBytes = readFileBytes(destBmp);
    QVERIFY(bmpBytes.size() > 2);
    QCOMPARE(bmpBytes.left(2), QByteArrayLiteral("BM"));

    // Test JPG conversion (\xFF\xD8 SOI marker)
    const QString destJpg = m_dir.filePath(QStringLiteral("out/test.jpg"));
    auto jpgRes = VtfConverter::convertToImageFile(
        FilesystemPath(m_vtfPath), FilesystemPath(destJpg), ImageFileFormat::Jpg);
    QVERIFY(jpgRes.isSuccess());
    QVERIFY(QFile::exists(destJpg));
    const auto jpgBytes = readFileBytes(destJpg);
    QVERIFY(jpgBytes.size() > 2);
    QCOMPARE(static_cast<unsigned char>(jpgBytes[0]), 0xFF);
    QCOMPARE(static_cast<unsigned char>(jpgBytes[1]), 0xD8);
}

void TestVtfConverter::formatExtensionMapping() {
    using Domain::Material::ImageFileFormat;
    QCOMPARE(VtfConverter::formatExtension(ImageFileFormat::Png), QStringLiteral("png"));
    QCOMPARE(VtfConverter::formatExtension(ImageFileFormat::Tga), QStringLiteral("tga"));
    QCOMPARE(VtfConverter::formatExtension(ImageFileFormat::Jpg), QStringLiteral("jpg"));
    QCOMPARE(VtfConverter::formatExtension(ImageFileFormat::Bmp), QStringLiteral("bmp"));
    QCOMPARE(VtfConverter::formatExtension(ImageFileFormat::Hdr), QStringLiteral("hdr"));
}

void TestVtfConverter::missingFileFails() {
    auto png = VtfConverter::convertToImageBuffer(FilesystemPath(m_dir.filePath(QStringLiteral("nope.vtf"))));
    QVERIFY(png.isFailure());
    QCOMPARE(png.errorCode(), Core::Error::ErrorCode::FileNotFound);
}

void TestVtfConverter::garbageFileFails() {
    auto png = VtfConverter::convertToImageBuffer(FilesystemPath(m_garbagePath));
    QVERIFY(png.isFailure());
}

QTEST_MAIN(TestVtfConverter)
#include "TestVtfConverter.moc"
