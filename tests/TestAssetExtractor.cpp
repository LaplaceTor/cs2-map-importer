#include <QTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Domain/Game/SearchTarget.h"
#include "Workflow/Common/AssetExtractor.h"
#include "Workflow/Common/CancellationToken.h"

#include "TestPackFixtures.h"

using namespace TestPackFixtures;
using Core::Path::FilesystemPath;
using Workflow::Common::AssetExtractOptions;
using Workflow::Common::AssetExtractor;
using Workflow::Common::CancellationToken;

namespace {

QByteArray readFileBytes(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

bool writeFileBytes(const QString& path, const QByteArray& data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(data) == data.size();
}

} // namespace

class TestAssetExtractor : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void extractsLooseFileFromDirectoryTarget();
    void extractsFromVpkTarget();
    void vpkLookupIsCaseInsensitive();
    void extractsFromPak01VpkInDirectoryTarget();
    void missingAssetIsSkipped();
    void cancelledTokenStopsExtraction();
    void extractsCompanionFiles();
    void emptyPathFails();

private:
    QTemporaryDir m_dir;
    QString m_looseDir;
    QString m_pakDir;
    QString m_vpkPath;
};

void TestAssetExtractor::initTestCase() {
    QVERIFY(m_dir.isValid());

    m_looseDir = m_dir.path() + QStringLiteral("/loose");
    QVERIFY(writeFileBytes(m_looseDir + QStringLiteral("/materials/loose.vmt"),
                           QByteArrayLiteral("loose vmt content")));

    m_pakDir = m_dir.path() + QStringLiteral("/pakdir");
    QVERIFY(createTestVpk(m_pakDir + QStringLiteral("/pak01_dir.vpk"), {
        {QStringLiteral("materials/pak.vmt"), QByteArrayLiteral("packed in pak01")},
    }));

    m_vpkPath = m_dir.filePath(QStringLiteral("assets_dir.vpk"));
    QVERIFY(createTestVpk(m_vpkPath, {
        {QStringLiteral("models/packed.mdl"), QByteArrayLiteral("packed mdl data")},
        {QStringLiteral("models/packed.vvd"), QByteArrayLiteral("packed vvd data")},
        {QStringLiteral("models/packed.sw.vtx"), QByteArrayLiteral("packed vtx data")},
    }));
}

void TestAssetExtractor::extractsLooseFileFromDirectoryTarget() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    const std::vector<Domain::Game::SearchTarget> targets = {
        Domain::Game::SearchTarget::makeDirectory(FilesystemPath(m_looseDir)),
    };

    auto result = AssetExtractor::extract(targets,
        QStringLiteral("materials/loose.vmt"), FilesystemPath(destDir.path()));
    QVERIFY(result.isSuccess());
    QVERIFY(!result.value().fromPack);
    QCOMPARE(result.value().sourceTargetPath.toString(), QDir(m_looseDir).absolutePath());
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("materials/loose.vmt"))),
             QByteArrayLiteral("loose vmt content"));
}

void TestAssetExtractor::extractsFromVpkTarget() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    const std::vector<Domain::Game::SearchTarget> targets = {
        Domain::Game::SearchTarget::makeVpk(FilesystemPath(m_vpkPath)),
    };

    auto result = AssetExtractor::extract(targets,
        QStringLiteral("models/packed.mdl"), FilesystemPath(destDir.path()));
    QVERIFY(result.isSuccess());
    QVERIFY(result.value().fromPack);
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("models/packed.mdl"))),
             QByteArrayLiteral("packed mdl data"));
}

void TestAssetExtractor::vpkLookupIsCaseInsensitive() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    const std::vector<Domain::Game::SearchTarget> targets = {
        Domain::Game::SearchTarget::makeVpk(FilesystemPath(m_vpkPath)),
    };

    auto result = AssetExtractor::extract(targets,
        QStringLiteral("MODELS/PACKED.MDL"), FilesystemPath(destDir.path()));
    QVERIFY(result.isSuccess());
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("models/packed.mdl"))),
             QByteArrayLiteral("packed mdl data"));
}

void TestAssetExtractor::extractsFromPak01VpkInDirectoryTarget() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    const std::vector<Domain::Game::SearchTarget> targets = {
        Domain::Game::SearchTarget::makeDirectory(FilesystemPath(m_pakDir)),
    };

    auto result = AssetExtractor::extract(targets,
        QStringLiteral("materials/pak.vmt"), FilesystemPath(destDir.path()));
    QVERIFY(result.isSuccess());
    QVERIFY(result.value().fromPack);
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("materials/pak.vmt"))),
             QByteArrayLiteral("packed in pak01"));
}

void TestAssetExtractor::missingAssetIsSkipped() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    const std::vector<Domain::Game::SearchTarget> targets = {
        Domain::Game::SearchTarget::makeDirectory(FilesystemPath(m_looseDir)),
        Domain::Game::SearchTarget::makeVpk(FilesystemPath(m_vpkPath)),
    };

    auto result = AssetExtractor::extract(targets,
        QStringLiteral("materials/absent.vmt"), FilesystemPath(destDir.path()));
    QVERIFY(result.isSkipped());
    QVERIFY(result.message().contains(QStringLiteral("absent.vmt")));
}

void TestAssetExtractor::cancelledTokenStopsExtraction() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    CancellationToken token;
    token.cancel();

    const std::vector<Domain::Game::SearchTarget> targets = {
        Domain::Game::SearchTarget::makeVpk(FilesystemPath(m_vpkPath)),
    };

    auto result = AssetExtractor::extract(targets,
        QStringLiteral("models/packed.mdl"), FilesystemPath(destDir.path()), {}, token);
    QVERIFY(result.isCancelled());
}

void TestAssetExtractor::extractsCompanionFiles() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    const std::vector<Domain::Game::SearchTarget> targets = {
        Domain::Game::SearchTarget::makeVpk(FilesystemPath(m_vpkPath)),
    };

    AssetExtractOptions options;
    options.companionExtensions = {QStringLiteral("vvd"), QStringLiteral("sw.vtx")};

    auto result = AssetExtractor::extract(targets,
        QStringLiteral("models/packed.mdl"), FilesystemPath(destDir.path()), options);
    QVERIFY(result.isSuccess());
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("models/packed.vvd"))),
             QByteArrayLiteral("packed vvd data"));
    QCOMPARE(readFileBytes(destDir.filePath(QStringLiteral("models/packed.sw.vtx"))),
             QByteArrayLiteral("packed vtx data"));
}

void TestAssetExtractor::emptyPathFails() {
    QTemporaryDir destDir;
    QVERIFY(destDir.isValid());

    const std::vector<Domain::Game::SearchTarget> targets = {
        Domain::Game::SearchTarget::makeVpk(FilesystemPath(m_vpkPath)),
    };

    auto emptyAsset = AssetExtractor::extract(targets,
        QString(), FilesystemPath(destDir.path()));
    QVERIFY(emptyAsset.isFailure());
    QCOMPARE(emptyAsset.errorCode(), Core::Error::ErrorCode::InvalidArgument);

    auto emptyDest = AssetExtractor::extract(targets,
        QStringLiteral("models/packed.mdl"), FilesystemPath(QString()));
    QVERIFY(emptyDest.isFailure());
    QCOMPARE(emptyDest.errorCode(), Core::Error::ErrorCode::InvalidPath);
}

QTEST_MAIN(TestAssetExtractor)
#include "TestAssetExtractor.moc"
