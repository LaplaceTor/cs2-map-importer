#include <QTest>
#include "Domain/Asset/AssetPath.h"
#include "Domain/Asset/AssetType.h"
#include "Domain/Asset/AssetTypeDetector.h"
#include "Core/Path/FilesystemPath.h"

using namespace Domain::Asset;
using Core::Path::FilesystemPath;

class TestAsset : public QObject {
    Q_OBJECT

private slots:
    void testAssetPathValidation();
    void testAssetPathAccessors();
    void testAssetPathEqualityAndHash();
    void testAssetPathResolution();
    void testAssetPathFromFilesystemPath();
    void testAssetPathSanitization();
    void testAssetTypeDetector();
};

void TestAsset::testAssetPathValidation() {
    AssetPath empty;
    QVERIFY(empty.isEmpty());
    QVERIFY(!empty.isValid());
    QCOMPARE(empty.toString(), QString());

    AssetPath valid(QStringLiteral("materials/models/props/crate.vmat"));
    QVERIFY(!valid.isEmpty());
    QVERIFY(valid.isValid());
    QCOMPARE(valid.toString(), QStringLiteral("materials/models/props/crate.vmat"));

    // Backslash normalization
    AssetPath backslash(QStringLiteral("materials\\models\\props\\crate.vmat"));
    QVERIFY(backslash.isValid());
    QCOMPARE(backslash.toString(), QStringLiteral("materials/models/props/crate.vmat"));

    // Absolute path rejection
    AssetPath absolute1(QStringLiteral("/materials/crate.vmat"));
    QVERIFY(!absolute1.isValid());

    AssetPath absolute2(QStringLiteral("C:/materials/crate.vmat"));
    QVERIFY(!absolute2.isValid());

    // Relative parent/current path rejection
    AssetPath traversal1(QStringLiteral("../materials/crate.vmat"));
    QVERIFY(!traversal1.isValid());

    AssetPath traversal2(QStringLiteral("materials/./crate.vmat"));
    QVERIFY(!traversal2.isValid());

    AssetPath traversal3(QStringLiteral("materials/../crate.vmat"));
    QVERIFY(!traversal3.isValid());
}

void TestAsset::testAssetPathAccessors() {
    AssetPath path(QStringLiteral("materials/models/props/crate.vmat"));
    QCOMPARE(path.fileName(), QStringLiteral("crate.vmat"));
    QCOMPARE(path.extension(), QStringLiteral("vmat"));
    QCOMPARE(path.directory(), QStringLiteral("materials/models/props"));

    AssetPath rootFile(QStringLiteral("crate.vmat"));
    QCOMPARE(rootFile.fileName(), QStringLiteral("crate.vmat"));
    QCOMPARE(rootFile.extension(), QStringLiteral("vmat"));
    QCOMPARE(rootFile.directory(), QString());

    AssetPath invalid;
    QCOMPARE(invalid.fileName(), QString());
    QCOMPARE(invalid.extension(), QString());
    QCOMPARE(invalid.directory(), QString());
}

void TestAsset::testAssetPathEqualityAndHash() {
    AssetPath a(QStringLiteral("models/props/box.vmdl"));
    AssetPath b(QStringLiteral("models/props/box.vmdl"));
    AssetPath c(QStringLiteral("models/props/other.vmdl"));

    QVERIFY(a == b);
    QVERIFY(a != c);
    QVERIFY(!(a < b) && !(b < a));
    QVERIFY(a < c || c < a);

    QHash<AssetPath, int> map;
    map.insert(a, 42);
    QCOMPARE(map.value(b), 42);
}

void TestAsset::testAssetPathResolution() {
    FilesystemPath baseDir(QStringLiteral("C:/game/csgo"));
    AssetPath asset(QStringLiteral("materials/brick/wall01.vmat"));

    FilesystemPath resolved = asset.resolve(baseDir);
    QVERIFY(resolved.isValid());
    QCOMPARE(resolved.toString(), QStringLiteral("C:/game/csgo/materials/brick/wall01.vmat"));

    AssetPath invalid;
    QVERIFY(!invalid.resolve(baseDir).isValid());

    FilesystemPath invalidBase;
    QVERIFY(!asset.resolve(invalidBase).isValid());
}

void TestAsset::testAssetPathFromFilesystemPath() {
    FilesystemPath baseDir(QStringLiteral("C:/game/csgo"));
    FilesystemPath targetFile(QStringLiteral("C:/game/csgo/materials/brick/wall01.vmat"));

    auto opt = AssetPath::fromFilesystemPath(baseDir, targetFile);
    QVERIFY(opt.has_value());
    QCOMPARE(opt->toString(), QStringLiteral("materials/brick/wall01.vmat"));

    // Outside base directory
    FilesystemPath outsideFile(QStringLiteral("C:/other/materials/brick/wall01.vmat"));
    auto outsideOpt = AssetPath::fromFilesystemPath(baseDir, outsideFile);
    QVERIFY(!outsideOpt.has_value());
}

void TestAsset::testAssetPathSanitization() {
    QString unsafeAsset = QStringLiteral("materials/{fence}#`|^.vmat");
    QString sanitized = AssetPath::sanitizeAssetName(unsafeAsset);
    QCOMPARE(sanitized, QStringLiteral("materials/_fence_____.vmat"));
}

void TestAsset::testAssetTypeDetector() {
    // Model detection
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("models/player.mdl"))), AssetType::Model);
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("models/player.vmdl"))), AssetType::Model);
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("models/player.smd"))), AssetType::Model);
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("models/player.fbx"))), AssetType::Model);

    // Particle detection
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("particles/fire.pcf"))), AssetType::Particle);
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("particles/fire.vpcf"))), AssetType::Particle);

    // Material detection
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("materials/wall.vmt"))), AssetType::Material);
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("materials/wall.vmat"))), AssetType::Material);
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("materials/wall.vtf"))), AssetType::Material);

    // Map detection
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("maps/de_dust2.vmf"))), AssetType::Map);
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("maps/de_dust2.bsp"))), AssetType::Map);
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("maps/de_dust2.vmap"))), AssetType::Map);

    // Unknown detection
    QCOMPARE(AssetTypeDetector::detect(AssetPath(QStringLiteral("scripts/game_sounds.txt"))), AssetType::Unknown);
    QCOMPARE(AssetTypeDetector::detectFromExtension(QStringLiteral("unknown")), AssetType::Unknown);
}

QTEST_MAIN(TestAsset)
#include "TestAsset.moc"
