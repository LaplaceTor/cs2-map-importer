#include <QTest>
#include <QFile>
#include <QTemporaryDir>

#include <bsppp/BSP.h>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Workflow/Common/BspEmbeddedExtractor.h"
#include "Core/Async/CancellationToken.h"

#include "TestPackFixtures.h"

using namespace TestPackFixtures;
using Core::Path::FilesystemPath;
using Workflow::Common::BspEmbeddedExtractor;
using Core::Async::CancellationToken;

namespace {

QByteArray readFileBytes(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

} // namespace

class TestBspEmbeddedExtractor : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void extractsAllEmbeddedFiles();
    void extractsManyEmbeddedFilesConcurrently();
    void bspWithoutPackIsSkipped();
    void missingBspFails();
    void cancelledTokenReturnsPartial();

private:
    QTemporaryDir m_dir;
    QString m_bspPath;
    QString m_largeBspPath;
    QString m_emptyBspPath;
};

void TestBspEmbeddedExtractor::initTestCase() {
    QVERIFY(m_dir.isValid());

    m_bspPath = m_dir.filePath(QStringLiteral("map.bsp"));
    QVERIFY(createTestBsp(m_bspPath, {
        {QStringLiteral("materials/embedded.vmt"), QByteArrayLiteral("embedded vmt content")},
        {QStringLiteral("sound/ambience.wav"), QByteArrayLiteral("wav bytes")},
    }));

    m_largeBspPath = m_dir.filePath(QStringLiteral("large.bsp"));
    PackEntryList largeEntries;
    largeEntries.reserve(60);
    for (int i = 0; i < 60; ++i) {
        QString path = QStringLiteral("materials/deep/folder%1/asset_%2.vmt").arg(i % 5).arg(i);
        QByteArray content = QStringLiteral("content of asset %1 with some extra payload").arg(i).toUtf8();
        largeEntries.emplace_back(std::move(path), std::move(content));
    }
    QVERIFY(createTestBsp(m_largeBspPath, largeEntries));

    m_emptyBspPath = m_dir.filePath(QStringLiteral("empty.bsp"));
    QVERIFY(createTestBsp(m_emptyBspPath, {}));
}

void TestBspEmbeddedExtractor::extractsAllEmbeddedFiles() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    auto result = BspEmbeddedExtractor::extract(FilesystemPath(m_bspPath), FilesystemPath(destDir.path()));
    QVERIFY(result.isSuccess());
    QCOMPARE(result.value(), std::size_t{2});
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("materials/embedded.vmt"))),
             QByteArrayLiteral("embedded vmt content"));
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("sound/ambience.wav"))),
             QByteArrayLiteral("wav bytes"));
}

void TestBspEmbeddedExtractor::extractsManyEmbeddedFilesConcurrently() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    auto result = BspEmbeddedExtractor::extract(FilesystemPath(m_largeBspPath), FilesystemPath(destDir.path()));
    QVERIFY(result.isSuccess());
    QCOMPARE(result.value(), std::size_t{60});

    // Verify sample files from different threads/folders
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("materials/deep/folder0/asset_0.vmt"))),
             QByteArrayLiteral("content of asset 0 with some extra payload"));
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("materials/deep/folder3/asset_33.vmt"))),
             QByteArrayLiteral("content of asset 33 with some extra payload"));
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("materials/deep/folder4/asset_59.vmt"))),
             QByteArrayLiteral("content of asset 59 with some extra payload"));
}

void TestBspEmbeddedExtractor::bspWithoutPackIsSkipped() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    auto result = BspEmbeddedExtractor::extract(FilesystemPath(m_emptyBspPath), FilesystemPath(destDir.path()));
    QVERIFY(result.isSkipped());
}

void TestBspEmbeddedExtractor::missingBspFails() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    auto result = BspEmbeddedExtractor::extract(
        FilesystemPath(m_dir.filePath(QStringLiteral("nope.bsp"))), FilesystemPath(destDir.path()));
    QVERIFY(result.isFailure());
    QCOMPARE(result.errorCode(), Core::Error::ErrorCode::FileNotFound);
}

void TestBspEmbeddedExtractor::cancelledTokenReturnsPartial() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    CancellationToken token;
    token.cancel();

    auto result = BspEmbeddedExtractor::extract(FilesystemPath(m_bspPath), FilesystemPath(destDir.path()), token);
    QVERIFY(result.isCancelled());
}

QTEST_MAIN(TestBspEmbeddedExtractor)
#include "TestBspEmbeddedExtractor.moc"
