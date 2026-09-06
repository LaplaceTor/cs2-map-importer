#include <algorithm>

#include <QTest>
#include <QFile>
#include <QTemporaryDir>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Domain/Package/PackArchive.h"

#include "TestPackFixtures.h"

using namespace TestPackFixtures;
using Core::Path::FilesystemPath;
using Domain::Package::PackArchive;

namespace {

bool entryListContains(const std::vector<QString>& entries, const QString& entryPath) {
    return std::find(entries.begin(), entries.end(), entryPath) != entries.end();
}

QByteArray readFileBytes(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

} // namespace

class TestPackArchive : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void openMissingFileFails();
    void openGarbageFails();
    void openVpkSucceeds();
    void listEntriesReturnsAll();
    void hasEntryIsCaseInsensitive();
    void readEntryReturnsBytes();
    void readEntryMissingFails();
    void extractEntryToFile();
    void extractAllToDirectory();
    void openBspEmbeddedPack();
    void readBspEntry();

private:
    QTemporaryDir m_dir;
    QString m_vpkPath;
    QString m_bspPath;
    QString m_garbagePath;
};

void TestPackArchive::initTestCase() {
    QVERIFY(m_dir.isValid());

    m_vpkPath = m_dir.filePath(QStringLiteral("test_dir.vpk"));
    QVERIFY(createTestVpk(m_vpkPath, {
        {QStringLiteral("materials/test.vmt"), QByteArrayLiteral("test vmt content")},
        {QStringLiteral("models/weapons/rifle.mdl"), QByteArrayLiteral("rifle mdl data")},
    }));

    m_bspPath = m_dir.filePath(QStringLiteral("test.bsp"));
    QVERIFY(createTestBsp(m_bspPath, {
        {QStringLiteral("materials/embedded.vmt"), QByteArrayLiteral("embedded vmt content")},
        {QStringLiteral("sound/ambience.wav"), QByteArrayLiteral("wav bytes")},
    }));

    m_garbagePath = m_dir.filePath(QStringLiteral("garbage.vpk"));
    QFile garbage(m_garbagePath);
    QVERIFY(garbage.open(QIODevice::WriteOnly | QIODevice::Truncate));
    garbage.write("this is not a vpk archive");
    garbage.close();
}

void TestPackArchive::openMissingFileFails() {
    auto archive = PackArchive::open(FilesystemPath(m_dir.filePath(QStringLiteral("nope.vpk"))));
    QVERIFY(archive.isFailure());
    QCOMPARE(archive.errorCode(), Core::Error::ErrorCode::FileNotFound);
}

void TestPackArchive::openGarbageFails() {
    auto archive = PackArchive::open(FilesystemPath(m_garbagePath));
    QVERIFY(archive.isFailure());
    QCOMPARE(archive.errorCode(), Core::Error::ErrorCode::InvalidFile);
}

void TestPackArchive::openVpkSucceeds() {
    auto archive = PackArchive::open(FilesystemPath(m_vpkPath));
    QVERIFY(archive.isSuccess());
    QVERIFY(archive.value().isOpen());
}

void TestPackArchive::listEntriesReturnsAll() {
    auto archive = PackArchive::open(FilesystemPath(m_vpkPath));
    QVERIFY(archive.isSuccess());

    auto entries = archive.value().listEntries();
    QVERIFY(entries.isSuccess());
    QVERIFY(entries.value().size() >= 2);
    QVERIFY(entryListContains(entries.value(), QStringLiteral("materials/test.vmt")));
    QVERIFY(entryListContains(entries.value(), QStringLiteral("models/weapons/rifle.mdl")));
}

void TestPackArchive::hasEntryIsCaseInsensitive() {
    auto archive = PackArchive::open(FilesystemPath(m_vpkPath));
    QVERIFY(archive.isSuccess());

    QVERIFY(archive.value().hasEntry(QStringLiteral("materials/test.vmt")));
    QVERIFY(archive.value().hasEntry(QStringLiteral("MATERIALS/TEST.VMT")));
    QVERIFY(archive.value().hasEntry(QStringLiteral("Materials/Test.Vmt")));
    QVERIFY(!archive.value().hasEntry(QStringLiteral("materials/absent.vmt")));
}

void TestPackArchive::readEntryReturnsBytes() {
    auto archive = PackArchive::open(FilesystemPath(m_vpkPath));
    QVERIFY(archive.isSuccess());

    auto data = archive.value().readEntry(QStringLiteral("materials/test.vmt"));
    QVERIFY(data.isSuccess());
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(data.value().data()),
                        static_cast<qsizetype>(data.value().size())),
             QByteArrayLiteral("test vmt content"));
}

void TestPackArchive::readEntryMissingFails() {
    auto archive = PackArchive::open(FilesystemPath(m_vpkPath));
    QVERIFY(archive.isSuccess());

    auto data = archive.value().readEntry(QStringLiteral("materials/absent.vmt"));
    QVERIFY(data.isFailure());
    QCOMPARE(data.errorCode(), Core::Error::ErrorCode::FileNotFound);
}

void TestPackArchive::extractEntryToFile() {
    auto archive = PackArchive::open(FilesystemPath(m_vpkPath));
    QVERIFY(archive.isSuccess());

    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());
    const QString destFile = destDir.filePath(QStringLiteral("rifle.mdl"));

    auto extracted = archive.value().extractEntryToFile(
        QStringLiteral("models/weapons/rifle.mdl"), FilesystemPath(destFile));
    QVERIFY(extracted.isSuccess());
    QCOMPARE(readFileBytes(destFile), QByteArrayLiteral("rifle mdl data"));
}

void TestPackArchive::extractAllToDirectory() {
    auto archive = PackArchive::open(FilesystemPath(m_vpkPath));
    QVERIFY(archive.isSuccess());

    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    auto extracted = archive.value().extractAllToDirectory(FilesystemPath(destDir.path()));
    QVERIFY(extracted.isSuccess());
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("materials/test.vmt"))),
             QByteArrayLiteral("test vmt content"));
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("models/weapons/rifle.mdl"))),
             QByteArrayLiteral("rifle mdl data"));
}

void TestPackArchive::openBspEmbeddedPack() {
    auto archive = PackArchive::open(FilesystemPath(m_bspPath));
    QVERIFY(archive.isSuccess());
    QVERIFY(archive.value().isOpen());

    auto entries = archive.value().listEntries();
    QVERIFY(entries.isSuccess());
    QVERIFY(entryListContains(entries.value(), QStringLiteral("materials/embedded.vmt")));
    QVERIFY(entryListContains(entries.value(), QStringLiteral("sound/ambience.wav")));
}

void TestPackArchive::readBspEntry() {
    auto archive = PackArchive::open(FilesystemPath(m_bspPath));
    QVERIFY(archive.isSuccess());

    auto data = archive.value().readEntry(QStringLiteral("sound/ambience.wav"));
    QVERIFY(data.isSuccess());
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(data.value().data()),
                        static_cast<qsizetype>(data.value().size())),
             QByteArrayLiteral("wav bytes"));
}

QTEST_MAIN(TestPackArchive)
#include "TestPackArchive.moc"
