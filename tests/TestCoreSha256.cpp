#include <QTest>
#include <QTemporaryFile>
#include "Core/Hash/Sha256.h"

class TestCoreSha256 : public QObject {
    Q_OBJECT

private slots:
    void testComputeDataHash() {
        QByteArray data = "hello world";
        // SHA-256 for "hello world": b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9
        QString hash = Core::Hash::Sha256::computeDataHash(data);
        QCOMPARE(hash, QStringLiteral("b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9"));
    }

    void testComputeFileHash() {
        QTemporaryFile tempFile;
        QVERIFY(tempFile.open());
        tempFile.write("hello world");
        tempFile.flush();
        tempFile.close();

        auto result = Core::Hash::Sha256::computeFileHash(Core::Path::FilesystemPath(tempFile.fileName()));
        QVERIFY(result.isSuccess());
        QCOMPARE(result.value(), QStringLiteral("b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9"));
    }

    void testFileNotFound() {
        auto result = Core::Hash::Sha256::computeFileHash(Core::Path::FilesystemPath(QStringLiteral("non_existent_file_path_123456.tmp")));
        QVERIFY(result.isFailure());
        QCOMPARE(result.error().code(), Core::Error::ErrorCode::FileNotFound);
    }
};

QTEST_MAIN(TestCoreSha256)
#include "TestCoreSha256.moc"
