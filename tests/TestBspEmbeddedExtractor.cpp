#include <QTest>
#include <QFile>
#include <QTemporaryDir>

#include <bsppp/BSP.h>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Workflow/Common/BspEmbeddedExtractor.h"
#include "Workflow/Common/CancellationToken.h"

#include "TestPackFixtures.h"

using namespace TestPackFixtures;
using Core::Path::FilesystemPath;
using Workflow::Common::BspEmbeddedExtractor;
using Workflow::Common::CancellationToken;

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
    void bspWithoutPackIsSkipped();
    void missingBspFails();
    void cancelledTokenReturnsPartial();

private:
    QTemporaryDir m_dir;
    QString m_bspPath;
    QString m_emptyBspPath;
};

void TestBspEmbeddedExtractor::initTestCase() {
    QVERIFY(m_dir.isValid());

    m_bspPath = m_dir.filePath(QStringLiteral("map.bsp"));
    QVERIFY(createTestBsp(m_bspPath, {
        {QStringLiteral("materials/embedded.vmt"), QByteArrayLiteral("embedded vmt content")},
        {QStringLiteral("sound/ambience.wav"), QByteArrayLiteral("wav bytes")},
    }));

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
