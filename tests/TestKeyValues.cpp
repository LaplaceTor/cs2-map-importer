#include <QTest>
#include <QTemporaryDir>
#include "Core/KeyValues/KeyValuesDocument.h"
#include "Core/KeyValues/KeyValuesParser.h"
#include "Core/KeyValues/KeyValuesWriter.h"
#include "Core/KeyValues/KeyValuesLexer.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Path/PathUtils.h"
#include "Core/Error/Exception.h"

using namespace Core::KeyValues;
using namespace Core::Path;

class TestKeyValues : public QObject {
    Q_OBJECT

private slots:
    void testBasicQuotedAndUnquoted();
    void testCommentsAndWhitespace();
    void testDuplicateKeysAndTraversal();
    void testDeepNesting();
    void testInPlaceModifications();
    void testSerializationRoundTrip();
    void testFileAtomicIO();
    void testErrorHandling();
    void testSpecialAndIllegalCharacters();
};

void TestKeyValues::testBasicQuotedAndUnquoted() {
    const QString kvText = QStringLiteral(
        "\"GameInfo\"\n"
        "{\n"
        "    game \"Counter-Strike: Global Offensive\"\n"
        "    title \"COUNTER-STRIKE\"\n"
        "    type multiplayer_only\n"
        "    skin 0\n"
        "    \"maxplayers\" 64\n"
        "    $basetexture materials/brick/wall.vtf\n"
        "}\n"
    );

    KeyValuesDocument doc = KeyValuesDocument::fromString(kvText);
    const KeyValuesNode* gameInfo = doc.findChild(QStringLiteral("GameInfo"));
    QVERIFY(gameInfo != nullptr);
    QVERIFY(gameInfo->isSection());

    QCOMPARE(gameInfo->property(QStringLiteral("game")), QStringLiteral("Counter-Strike: Global Offensive"));
    QCOMPARE(gameInfo->property(QStringLiteral("title")), QStringLiteral("COUNTER-STRIKE"));
    QCOMPARE(gameInfo->property(QStringLiteral("type")), QStringLiteral("multiplayer_only"));
    QCOMPARE(gameInfo->propertyInt(QStringLiteral("skin")), 0);
    QCOMPARE(gameInfo->propertyInt(QStringLiteral("maxplayers")), 64);
    QCOMPARE(gameInfo->property(QStringLiteral("$basetexture")), QStringLiteral("materials/brick/wall.vtf"));
}

void TestKeyValues::testCommentsAndWhitespace() {
    const QString kvText = QStringLiteral(
        "// File header comment\n"
        "\"Layer0\"\n"
        "{\n"
        "    // Shader declaration\n"
        "    \"shader\" \"sky.vfx\"\n"
        "    \n"
        "    \"SkyTexture\" \"materials/skybox/custom_cube.jpg\" // inline comment\n"
        "    F_FORCE_UV2 1\n"
        "}\n"
    );

    KeyValuesDocument doc = KeyValuesDocument::fromString(kvText);
    const KeyValuesNode* layer = doc.findChild(QStringLiteral("Layer0"));
    QVERIFY(layer != nullptr);

    QCOMPARE(layer->property(QStringLiteral("shader")), QStringLiteral("sky.vfx"));
    QCOMPARE(layer->property(QStringLiteral("SkyTexture")), QStringLiteral("materials/skybox/custom_cube.jpg"));
    QCOMPARE(layer->propertyInt(QStringLiteral("F_FORCE_UV2")), 1);
    QCOMPARE(layer->propertyBool(QStringLiteral("F_FORCE_UV2")), true);
}

void TestKeyValues::testDuplicateKeysAndTraversal() {
    const QString kvText = QStringLiteral(
        "entity\n"
        "{\n"
        "    id 1\n"
        "    classname info_player_terrorist\n"
        "    origin \"0 0 0\"\n"
        "}\n"
        "entity\n"
        "{\n"
        "    id 2\n"
        "    classname info_player_counterterrorist\n"
        "    origin \"100 200 0\"\n"
        "}\n"
        "entity\n"
        "{\n"
        "    id 3\n"
        "    classname env_soundscape\n"
        "    soundscape \"custom.sound\"\n"
        "}\n"
    );

    KeyValuesDocument doc = KeyValuesDocument::fromString(kvText);
    auto entities = doc.findChildren(QStringLiteral("entity"));
    QCOMPARE(static_cast<int>(entities.size()), 3);

    QCOMPARE(entities[0]->propertyInt(QStringLiteral("id")), 1);
    QCOMPARE(entities[0]->property(QStringLiteral("classname")), QStringLiteral("info_player_terrorist"));

    QCOMPARE(entities[1]->propertyInt(QStringLiteral("id")), 2);
    QCOMPARE(entities[1]->property(QStringLiteral("classname")), QStringLiteral("info_player_counterterrorist"));

    QCOMPARE(entities[2]->propertyInt(QStringLiteral("id")), 3);
    QCOMPARE(entities[2]->property(QStringLiteral("classname")), QStringLiteral("env_soundscape"));
}

void TestKeyValues::testDeepNesting() {
    const QString vmfSnippet = QStringLiteral(
        "world\n"
        "{\n"
        "    id 1\n"
        "    solid\n"
        "    {\n"
        "        id 2\n"
        "        side\n"
        "        {\n"
        "            id 3\n"
        "            plane \"( -64 -64 0 ) ( -64 64 0 ) ( 64 64 0 )\"\n"
        "            material \"DEV/DEV_MEASUREGENERIC01B\"\n"
        "            dispinfo\n"
        "            {\n"
        "                power 3\n"
        "                elevation 0\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}\n"
    );

    KeyValuesDocument doc = KeyValuesDocument::fromString(vmfSnippet);
    const KeyValuesNode* world = doc.findChild(QStringLiteral("world"));
    QVERIFY(world != nullptr);

    const KeyValuesNode* solid = world->findChild(QStringLiteral("solid"));
    QVERIFY(solid != nullptr);
    QCOMPARE(solid->propertyInt(QStringLiteral("id")), 2);

    const KeyValuesNode* side = solid->findChild(QStringLiteral("side"));
    QVERIFY(side != nullptr);
    QCOMPARE(side->property(QStringLiteral("material")), QStringLiteral("DEV/DEV_MEASUREGENERIC01B"));

    const KeyValuesNode* dispinfo = side->findChild(QStringLiteral("dispinfo"));
    QVERIFY(dispinfo != nullptr);
    QCOMPARE(dispinfo->propertyInt(QStringLiteral("power")), 3);
}

void TestKeyValues::testInPlaceModifications() {
    const QString kvText = QStringLiteral(
        "\"Layer0\"\n"
        "{\n"
        "    \"shader\" \"complex.vfx\"\n"
        "    \"$translucent\" \"1\"\n"
        "}\n"
    );

    KeyValuesDocument doc = KeyValuesDocument::fromString(kvText);
    KeyValuesNode* layer = doc.findChild(QStringLiteral("Layer0"));
    QVERIFY(layer != nullptr);

    // Add properties
    layer->addProperty(QStringLiteral("F_TRANSLUCENT"), QStringLiteral("1"));
    layer->setProperty(QStringLiteral("shader"), QStringLiteral("csgo_complex.vfx"));

    QCOMPARE(layer->property(QStringLiteral("shader")), QStringLiteral("csgo_complex.vfx"));
    QCOMPARE(layer->property(QStringLiteral("F_TRANSLUCENT")), QStringLiteral("1"));

    // Remove legacy translucent flag
    int removed = layer->removeProperties(QStringLiteral("$translucent"));
    QCOMPARE(removed, 1);
    QVERIFY(!layer->hasProperty(QStringLiteral("$translucent")));
}

void TestKeyValues::testSerializationRoundTrip() {
    KeyValuesDocument originalDoc;
    KeyValuesNode& section = originalDoc.addSection(QStringLiteral("TestBlock"));
    section.addProperty(QStringLiteral("Key1"), QStringLiteral("Value One"));
    section.addProperty(QStringLiteral("Key2"), QStringLiteral("123"));

    KeyValuesNode& subSection = section.addSection(QStringLiteral("SubBlock"));
    subSection.addProperty(QStringLiteral("SubKey"), QStringLiteral("SubValue"));

    // Serialize
    const QString serialized = originalDoc.saveToString();

    // Re-parse
    KeyValuesDocument reloadedDoc = KeyValuesDocument::fromString(serialized);
    const KeyValuesNode* reloadedSection = reloadedDoc.findChild(QStringLiteral("TestBlock"));
    QVERIFY(reloadedSection != nullptr);
    QCOMPARE(reloadedSection->property(QStringLiteral("Key1")), QStringLiteral("Value One"));
    QCOMPARE(reloadedSection->propertyInt(QStringLiteral("Key2")), 123);

    const KeyValuesNode* reloadedSub = reloadedSection->findChild(QStringLiteral("SubBlock"));
    QVERIFY(reloadedSub != nullptr);
    QCOMPARE(reloadedSub->property(QStringLiteral("SubKey")), QStringLiteral("SubValue"));
}

void TestKeyValues::testFileAtomicIO() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    FilesystemPath filePath(tempDir.filePath(QStringLiteral("test_asset.vdf")));

    KeyValuesDocument doc;
    KeyValuesNode& root = doc.addSection(QStringLiteral("RootConfig"));
    root.addProperty(QStringLiteral("TargetGame"), QStringLiteral("csgo"));
    root.addProperty(QStringLiteral("Enabled"), QStringLiteral("1"));

    QVERIFY(doc.saveToFile(filePath));
    QVERIFY(filePath.exists());

    KeyValuesDocument loadedDoc = KeyValuesDocument::fromFile(filePath);
    const KeyValuesNode* rootNode = loadedDoc.findChild(QStringLiteral("RootConfig"));
    QVERIFY(rootNode != nullptr);
    QCOMPARE(rootNode->property(QStringLiteral("TargetGame")), QStringLiteral("csgo"));
    QCOMPARE(rootNode->propertyBool(QStringLiteral("Enabled")), true);
}

void TestKeyValues::testErrorHandling() {
    // Unclosed brace
    const QString broken1 = QStringLiteral("Root { key value");
    KeyValuesParser parser;
    KeyValuesNode root;
    QString errorMsg;
    QVERIFY(!parser.parse(broken1, root, &errorMsg));
    QVERIFY(!errorMsg.isEmpty());

    // Throws exception
    QVERIFY_EXCEPTION_THROWN(parser.parseOrThrow(broken1), Core::Error::Exception);

    // Unexpected closing brace
    const QString broken2 = QStringLiteral("Root { } }");
    QVERIFY(!parser.parse(broken2, root, &errorMsg));
}

void TestKeyValues::testSpecialAndIllegalCharacters() {
    // 1. Unquoted materials and names with '{', '}', '~', '+', '*', '%', '$', '(', ')'
    const QString kvText = QStringLiteral(
        "world\n"
        "{\n"
        "    material {fence\n"
        "    custom_path materials/{ladder.vmt\n"
        "    animated_tex +0~light\n"
        "    brush_model *1\n"
        "    editor_mat %compiletrigger\n"
        "    targetname [PR#]{gate_trigger}\n"
        "}\n"
    );

    QString errorMsg;
    KeyValuesDocument doc;
    bool parsed = doc.loadFromString(kvText, &errorMsg);
    if (!parsed) {
        qWarning() << "Parse error in testSpecialAndIllegalCharacters:" << errorMsg;
    }
    QVERIFY2(parsed, qPrintable(errorMsg));
    const KeyValuesNode* world = doc.findChild(QStringLiteral("world"));
    QVERIFY(world != nullptr);

    QCOMPARE(world->property(QStringLiteral("material")), QStringLiteral("{fence"));
    QCOMPARE(world->property(QStringLiteral("custom_path")), QStringLiteral("materials/{ladder.vmt"));
    QCOMPARE(world->property(QStringLiteral("animated_tex")), QStringLiteral("+0~light"));
    QCOMPARE(world->property(QStringLiteral("brush_model")), QStringLiteral("*1"));
    QCOMPARE(world->property(QStringLiteral("editor_mat")), QStringLiteral("%compiletrigger"));
    QCOMPARE(world->property(QStringLiteral("targetname")), QStringLiteral("[PR#]{gate_trigger}"));

    // 2. Serialization properly double-quotes tokens with braces
    const QString serialized = doc.saveToString();
    QVERIFY(serialized.contains(QStringLiteral("\"{fence\"")));
    QVERIFY(serialized.contains(QStringLiteral("\"materials/{ladder.vmt\"")));

    // 3. PathUtils sanitization
    QString unsafeWinFilename = QStringLiteral("test<file>:name*?.vmt");
    QString sanitizedWin = PathUtils::sanitizeFilename(unsafeWinFilename);
    QCOMPARE(sanitizedWin, QStringLiteral("test_file__name__.vmt"));
}

QTEST_MAIN(TestKeyValues)
#include "TestKeyValues.moc"

