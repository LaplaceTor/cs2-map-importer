#include <QTest>
#include <QDir>
#include <QFileInfo>
#include "Domain/Game/GameInfo.h"
#include "Domain/Game/GameInfoParser.h"
#include "Domain/Game/SearchPathResolver.h"
#include "Domain/Game/SearchTarget.h"
#include "Domain/Game/GameType.h"
#include "Domain/Game/GameDefinition.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameValidator.h"
#include "Core/Path/FilesystemPath.h"

using namespace Domain::Game;

class TestGameInfo : public QObject {
    Q_OBJECT

private:
    QString m_testFilesRoot;

private slots:
    void initTestCase() {
        // Locate gameinfotestfile directory relative to application source directory or current path
        QString candidate = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../../gameinfotestfile"));
        if (!QDir(candidate).exists()) {
            candidate = QDir::current().filePath(QStringLiteral("gameinfotestfile"));
        }
        if (!QDir(candidate).exists()) {
            candidate = QStringLiteral(GAMEINFO_TEST_DIR);
        }
        m_testFilesRoot = QDir::cleanPath(candidate);
        QVERIFY2(QDir(m_testFilesRoot).exists(), qPrintable(QStringLiteral("Test directory not found: %1").arg(m_testFilesRoot)));
    }

    void testSearchTargetValueObject() {
        Core::Path::FilesystemPath dirPath(QStringLiteral("C:/Games/hl2"));
        Core::Path::FilesystemPath vpkPath(QStringLiteral("C:/Games/hl2/hl2_pak_dir.vpk"));

        auto targetDir = SearchTarget::makeDirectory(dirPath);
        QVERIFY(targetDir.isDirectory());
        QVERIFY(!targetDir.isVpk());
        QCOMPARE(targetDir.type(), SearchTargetType::Directory);
        QCOMPARE(targetDir.path(), dirPath);
        QCOMPARE(targetDir.pathString(), QStringLiteral("C:/Games/hl2"));

        auto targetVpk = SearchTarget::makeVpk(vpkPath);
        QVERIFY(targetVpk.isVpk());
        QVERIFY(!targetVpk.isDirectory());
        QCOMPARE(targetVpk.type(), SearchTargetType::Vpk);
        QCOMPARE(targetVpk.path(), vpkPath);

        SearchTarget targetDir2(SearchTargetType::Directory, dirPath);
        QCOMPARE(targetDir, targetDir2);
        QVERIFY(targetDir != targetVpk);
    }

    void testCounterStrikeSource() {
        QString giPathStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source/cstrike/gameinfo.txt"));
        Core::Path::FilesystemPath giPath(giPathStr);

        QString error;
        auto result = GameInfoParser::parse(giPath, &error);
        QVERIFY2(result.has_value(), qPrintable(error));

        const auto& info = *result;
        QCOMPARE(info.game(), QStringLiteral("Counter-Strike Source"));
        QCOMPARE(info.steamAppId(), 240);

        QString expectedBaseDir = QDir::cleanPath(QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source")));
        QCOMPARE(info.baseDirectory().toString(), expectedBaseDir);
        QCOMPARE(info.modDirectory().toString(), QDir::cleanPath(QDir(expectedBaseDir).filePath(QStringLiteral("cstrike"))));

        const auto& targets = info.searchTargets();
        QVERIFY(!targets.empty());

        // First target must be the mod directory
        QCOMPARE(targets[0].path().toString(), QDir::cleanPath(QDir(expectedBaseDir).filePath(QStringLiteral("cstrike"))));
        QVERIFY(targets[0].isDirectory());

        // Check VPKs normalization (_dir.vpk)
        bool hasCstrikePak = false;
        bool hasHl2Textures = false;
        bool hasCustomWildcard = false;

        for (const auto& t : targets) {
            if (t.pathString().contains(QStringLiteral("cstrike_pak_dir.vpk"))) {
                hasCstrikePak = true;
                QVERIFY(t.isVpk());
            }
            if (t.pathString().contains(QStringLiteral("hl2_textures_dir.vpk"))) {
                hasHl2Textures = true;
                QVERIFY(t.isVpk());
            }
            if (t.pathString().contains(QStringLiteral("custom/*")) || t.pathString().endsWith(QStringLiteral("custom"))) {
                hasCustomWildcard = true;
            }
        }

        QVERIFY(hasCstrikePak);
        QVERIFY(hasHl2Textures);
        QVERIFY(!hasCustomWildcard); // wildcard 'cstrike/custom/*' should have been skipped
    }

    void testHalfLife2() {
        QString giPathStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Half-Life 2/hl2/gameinfo.txt"));
        Core::Path::FilesystemPath giPath(giPathStr);

        QString error;
        auto result = GameInfoParser::parse(giPath, &error);
        QVERIFY2(result.has_value(), qPrintable(error));

        const auto& info = *result;
        QCOMPARE(info.game(), QStringLiteral("HALF-LIFE 2"));
        QCOMPARE(info.steamAppId(), 220);

        QString expectedBaseDir = QDir::cleanPath(QDir(m_testFilesRoot).filePath(QStringLiteral("Half-Life 2")));
        QCOMPARE(info.baseDirectory().toString(), expectedBaseDir);
        QCOMPARE(info.modDirectory().toString(), QDir::cleanPath(QDir(expectedBaseDir).filePath(QStringLiteral("hl2"))));

        const auto& targets = info.searchTargets();
        QVERIFY(!targets.empty());

        bool hasHl2Lv = false;
        bool hasHl2Pak = false;
        for (const auto& t : targets) {
            if (t.pathString().contains(QStringLiteral("hl2_lv_dir.vpk"))) hasHl2Lv = true;
            if (t.pathString().contains(QStringLiteral("hl2_pak_dir.vpk"))) hasHl2Pak = true;
        }
        QVERIFY(hasHl2Lv);
        QVERIFY(hasHl2Pak);
    }

    void testLeft4Dead2() {
        QString giPathStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Left 4 Dead 2/left4dead2/gameinfo.txt"));
        Core::Path::FilesystemPath giPath(giPathStr);

        QString error;
        auto result = GameInfoParser::parse(giPath, &error);
        QVERIFY2(result.has_value(), qPrintable(error));

        const auto& info = *result;
        QCOMPARE(info.game(), QStringLiteral("Left 4 Dead 2"));
        QCOMPARE(info.steamAppId(), 550);
        QCOMPARE(info.toolsAppId(), 563);

        QString expectedBaseDir = QDir::cleanPath(QDir(m_testFilesRoot).filePath(QStringLiteral("Left 4 Dead 2")));
        QCOMPARE(info.baseDirectory().toString(), expectedBaseDir);

        const auto& targets = info.searchTargets();
        QVERIFY(!targets.empty());

        bool hasDlc3 = false;
        bool hasUpdate = false;
        for (const auto& t : targets) {
            if (t.pathString().contains(QStringLiteral("left4dead2_dlc3"))) hasDlc3 = true;
            if (t.pathString().contains(QStringLiteral("update"))) hasUpdate = true;
        }
        QVERIFY(hasDlc3);
        QVERIFY(hasUpdate);
    }

    void testPortal2() {
        QString giPathStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Portal 2/portal2/gameinfo.txt"));
        Core::Path::FilesystemPath giPath(giPathStr);

        QString error;
        auto result = GameInfoParser::parse(giPath, &error);
        QVERIFY2(result.has_value(), qPrintable(error));

        const auto& info = *result;
        QCOMPARE(info.game(), QStringLiteral("PORTAL 2"));
        QCOMPARE(info.steamAppId(), 620);
        QCOMPARE(info.toolsAppId(), 211);

        QString expectedBaseDir = QDir::cleanPath(QDir(m_testFilesRoot).filePath(QStringLiteral("Portal 2")));
        QCOMPARE(info.baseDirectory().toString(), expectedBaseDir);

        const auto& targets = info.searchTargets();
        QCOMPARE(targets.size(), static_cast<size_t>(1));
        QCOMPARE(targets[0].path().toString(), QDir::cleanPath(QDir(expectedBaseDir).filePath(QStringLiteral("portal2"))));
    }

    void testTeamFortress2() {
        QString giPathStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Team Fortress 2/tf/gameinfo.txt"));
        Core::Path::FilesystemPath giPath(giPathStr);

        QString error;
        auto result = GameInfoParser::parse(giPath, &error);
        QVERIFY2(result.has_value(), qPrintable(error));

        const auto& info = *result;
        QCOMPARE(info.game(), QStringLiteral("Team Fortress 2"));
        QCOMPARE(info.steamAppId(), 440);

        QString expectedBaseDir = QDir::cleanPath(QDir(m_testFilesRoot).filePath(QStringLiteral("Team Fortress 2")));
        QCOMPARE(info.baseDirectory().toString(), expectedBaseDir);

        const auto& targets = info.searchTargets();
        QVERIFY(!targets.empty());

        bool hasTf2Textures = false;
        bool hasTfBin = false;
        for (const auto& t : targets) {
            if (t.pathString().contains(QStringLiteral("tf2_textures_dir.vpk"))) hasTf2Textures = true;
            if (t.pathString().endsWith(QStringLiteral("tf/bin"))) hasTfBin = true;
        }
        QVERIFY(hasTf2Textures);
        QVERIFY(hasTfBin);
    }

    void testCsgoLegacy() {
        QString giPathStr = QDir(m_testFilesRoot).filePath(QStringLiteral("csgo legacy/csgo/gameinfo.txt"));
        Core::Path::FilesystemPath giPath(giPathStr);

        QString error;
        auto result = GameInfoParser::parse(giPath, &error);
        QVERIFY2(result.has_value(), qPrintable(error));

        const auto& info = *result;
        QCOMPARE(info.game(), QStringLiteral("Counter-Strike: Global Offensive"));
        QCOMPARE(info.steamAppId(), 730);
        QCOMPARE(info.toolsAppId(), 211);

        QString expectedBaseDir = QDir::cleanPath(QDir(m_testFilesRoot).filePath(QStringLiteral("csgo legacy")));
        QCOMPARE(info.baseDirectory().toString(), expectedBaseDir);

        const auto& targets = info.searchTargets();
        QCOMPARE(targets.size(), static_cast<size_t>(1));
        QCOMPARE(targets[0].path().toString(), QDir::cleanPath(QDir(expectedBaseDir).filePath(QStringLiteral("csgo"))));
    }

    void testParseFromString() {
        QString content = QStringLiteral(
            "\"GameInfo\"\n"
            "{\n"
            "    game \"Custom Game\"\n"
            "    FileSystem\n"
            "    {\n"
            "        SteamAppId 1000\n"
            "        SearchPaths\n"
            "        {\n"
            "            game+game_write custom_mod\n"
            "            game custom_mod/pak01.vpk\n"
            "            game shared/loose\n"
            "        }\n"
            "    }\n"
            "}\n"
        );

        Core::Path::FilesystemPath giPath(QStringLiteral("C:/MyGames/Engine/custom_mod/gameinfo.txt"));
        QString error;
        auto result = GameInfoParser::parseFromString(content, giPath, &error);
        QVERIFY2(result.has_value(), qPrintable(error));

        const auto& info = *result;
        QCOMPARE(info.game(), QStringLiteral("Custom Game"));
        QCOMPARE(info.steamAppId(), 1000);
        QCOMPARE(info.baseDirectory().toString(), QStringLiteral("C:/MyGames/Engine"));
        QCOMPARE(info.modDirectory().toString(), QStringLiteral("C:/MyGames/Engine/custom_mod"));

        const auto& targets = info.searchTargets();
        QCOMPARE(targets.size(), static_cast<size_t>(3));
        QCOMPARE(targets[0].path().toString(), QStringLiteral("C:/MyGames/Engine/custom_mod"));
        QVERIFY(targets[0].isDirectory());
        QCOMPARE(targets[1].path().toString(), QStringLiteral("C:/MyGames/Engine/custom_mod/pak01_dir.vpk"));
        QVERIFY(targets[1].isVpk());
        QCOMPARE(targets[2].path().toString(), QStringLiteral("C:/MyGames/Engine/shared/loose"));
        QVERIFY(targets[2].isDirectory());
    }

    void testInvalidGameInfo() {
        Core::Path::FilesystemPath nonExistent(QStringLiteral("C:/non/existent/path/gameinfo.txt"));
        QString error;
        auto result = GameInfoParser::parse(nonExistent, &error);
        QVERIFY(!result.has_value());
        QVERIFY(!error.isEmpty());
    }

    void testGameRegistry() {
        const auto& defs = GameRegistry::allDefinitions();
        QVERIFY(defs.size() >= 10);

        const auto* cs2Def = GameRegistry::findByType(GameType::CS2);
        QVERIFY(cs2Def != nullptr);
        QCOMPARE(cs2Def->id, QStringLiteral("cs2"));
        QCOMPARE(cs2Def->primaryAppId, 730);
        QCOMPARE(cs2Def->gameInfoFileName, QStringLiteral("gameinfo.gi"));
        QCOMPARE(cs2Def->modSubdirectory, QStringLiteral("game/csgo"));
        QVERIFY(cs2Def->isSource2());

        const auto* cssDef = GameRegistry::findById(QStringLiteral("css"));
        QVERIFY(cssDef != nullptr);
        QCOMPARE(cssDef->type, GameType::CSS);
        QCOMPARE(cssDef->primaryAppId, 240);
        QVERIFY(!cssDef->isSource2());

        const auto* hl2Def = GameRegistry::findByAppId(220);
        QVERIFY(hl2Def != nullptr);
        QCOMPARE(hl2Def->type, GameType::HL2);

        QCOMPARE(GameRegistry::gameTypeToString(GameType::Portal2), QStringLiteral("portal2"));
        QCOMPARE(GameRegistry::stringToGameType(QStringLiteral("tf2")), GameType::TF2);
    }

    void testGameValidatorWithFixtures() {
        // Counter-Strike Source fixture
        QString cssDirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source"));
        auto cssValidation = GameValidator::validateDirectory(Core::Path::FilesystemPath(cssDirStr), GameType::CSS);
        QVERIFY(cssValidation.has_value());
        QCOMPARE(cssValidation->game(), QStringLiteral("Counter-Strike Source"));

        // Half-Life 2 fixture
        QString hl2DirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Half-Life 2"));
        auto hl2Validation = GameValidator::validateDirectory(Core::Path::FilesystemPath(hl2DirStr), GameType::HL2);
        QVERIFY(hl2Validation.has_value());
        QCOMPARE(hl2Validation->game(), QStringLiteral("HALF-LIFE 2"));

        // Portal 2 fixture
        QString portal2DirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Portal 2"));
        auto portal2Validation = GameValidator::validateDirectory(Core::Path::FilesystemPath(portal2DirStr), GameType::Portal2);
        QVERIFY(portal2Validation.has_value());
        QCOMPARE(portal2Validation->game(), QStringLiteral("PORTAL 2"));

        // Team Fortress 2 fixture
        QString tf2DirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Team Fortress 2"));
        auto tf2Validation = GameValidator::validateDirectory(Core::Path::FilesystemPath(tf2DirStr), GameType::TF2);
        QVERIFY(tf2Validation.has_value());
        QCOMPARE(tf2Validation->game(), QStringLiteral("Team Fortress 2"));

        // Left 4 Dead 2 fixture
        QString l4d2DirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Left 4 Dead 2"));
        auto l4d2Validation = GameValidator::validateDirectory(Core::Path::FilesystemPath(l4d2DirStr), GameType::L4D2);
        QVERIFY(l4d2Validation.has_value());
        QCOMPARE(l4d2Validation->game(), QStringLiteral("Left 4 Dead 2"));

        // CS:GO Legacy fixture
        QString csgoDirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("csgo legacy"));
        auto csgoValidation = GameValidator::validateDirectory(Core::Path::FilesystemPath(csgoDirStr), GameType::CSGO);
        QVERIFY(csgoValidation.has_value());
        QCOMPARE(csgoValidation->game(), QStringLiteral("Counter-Strike: Global Offensive"));

        // Negative test: validate Half-Life 2 as CS:S should fail
        auto wrongValidation = GameValidator::validateDirectory(Core::Path::FilesystemPath(hl2DirStr), GameType::CSS);
        QVERIFY(!wrongValidation.has_value());

        // Negative tests: Portal 2 should NOT validate as Portal, Left 4 Dead 2 should NOT validate as L4D
        QVERIFY(!GameValidator::validateGameInfo(*portal2Validation, GameType::Portal));
        QVERIFY(!GameValidator::validateGameInfo(*l4d2Validation, GameType::L4D));

        // Test auto-identification across all fixtures
        auto identifiedCss = GameValidator::identifyGameType(*cssValidation);
        QVERIFY(identifiedCss.has_value());
        QCOMPARE(*identifiedCss, GameType::CSS);

        auto identifiedHl2 = GameValidator::identifyGameType(*hl2Validation);
        QVERIFY(identifiedHl2.has_value());
        QCOMPARE(*identifiedHl2, GameType::HL2);

        auto identifiedPortal2 = GameValidator::identifyGameType(*portal2Validation);
        QVERIFY(identifiedPortal2.has_value());
        QCOMPARE(*identifiedPortal2, GameType::Portal2);
        QVERIFY(*identifiedPortal2 != GameType::Portal);

        auto identifiedL4d2 = GameValidator::identifyGameType(*l4d2Validation);
        QVERIFY(identifiedL4d2.has_value());
        QCOMPARE(*identifiedL4d2, GameType::L4D2);
        QVERIFY(*identifiedL4d2 != GameType::L4D);

        auto identifiedTf2 = GameValidator::identifyGameType(*tf2Validation);
        QVERIFY(identifiedTf2.has_value());
        QCOMPARE(*identifiedTf2, GameType::TF2);

        auto identifiedCsgo = GameValidator::identifyGameType(*csgoValidation);
        QVERIFY(identifiedCsgo.has_value());
        QCOMPARE(*identifiedCsgo, GameType::CSGO);
    }

    void testIdentifyGameTypeTieredResolution() {
        // 1. Exact title matching without AppID
        {
            QString content = QStringLiteral("\"GameInfo\" { game \"PORTAL 2\" }\n");
            auto parsed = GameInfoParser::parseFromString(content, Core::Path::FilesystemPath(QStringLiteral("C:/mock/gameinfo.txt")));
            QVERIFY(parsed.has_value());
            auto type = GameValidator::identifyGameType(*parsed);
            QVERIFY(type.has_value());
            QCOMPARE(*type, GameType::Portal2);
            QVERIFY(!GameValidator::validateGameInfo(*parsed, GameType::Portal));
        }
        {
            QString content = QStringLiteral("\"GameInfo\" { game \"Portal\" }\n");
            auto parsed = GameInfoParser::parseFromString(content, Core::Path::FilesystemPath(QStringLiteral("C:/mock/gameinfo.txt")));
            QVERIFY(parsed.has_value());
            auto type = GameValidator::identifyGameType(*parsed);
            QVERIFY(type.has_value());
            QCOMPARE(*type, GameType::Portal);
            QVERIFY(!GameValidator::validateGameInfo(*parsed, GameType::Portal2));
        }
        {
            QString content = QStringLiteral("\"GameInfo\" { game \"Left 4 Dead 2\" }\n");
            auto parsed = GameInfoParser::parseFromString(content, Core::Path::FilesystemPath(QStringLiteral("C:/mock/gameinfo.txt")));
            QVERIFY(parsed.has_value());
            auto type = GameValidator::identifyGameType(*parsed);
            QVERIFY(type.has_value());
            QCOMPARE(*type, GameType::L4D2);
            QVERIFY(!GameValidator::validateGameInfo(*parsed, GameType::L4D));
        }
        {
            QString content = QStringLiteral("\"GameInfo\" { game \"Left 4 Dead\" }\n");
            auto parsed = GameInfoParser::parseFromString(content, Core::Path::FilesystemPath(QStringLiteral("C:/mock/gameinfo.txt")));
            QVERIFY(parsed.has_value());
            auto type = GameValidator::identifyGameType(*parsed);
            QVERIFY(type.has_value());
            QCOMPARE(*type, GameType::L4D);
            QVERIFY(!GameValidator::validateGameInfo(*parsed, GameType::L4D2));
        }

        // 2. Exact alias matching
        {
            QString content = QStringLiteral("\"GameInfo\" { game \"Counter-Strike: Source\" }\n");
            auto parsed = GameInfoParser::parseFromString(content, Core::Path::FilesystemPath(QStringLiteral("C:/mock/gameinfo.txt")));
            QVERIFY(parsed.has_value());
            auto type = GameValidator::identifyGameType(*parsed);
            QVERIFY(type.has_value());
            QCOMPARE(*type, GameType::CSS);
        }

        // 3. Steam AppID matching
        {
            QString content = QStringLiteral("\"GameInfo\" { game \"Unknown Mod\" FileSystem { SteamAppId 620 } }\n");
            auto parsed = GameInfoParser::parseFromString(content, Core::Path::FilesystemPath(QStringLiteral("C:/mock/gameinfo.txt")));
            QVERIFY(parsed.has_value());
            auto type = GameValidator::identifyGameType(*parsed);
            QVERIFY(type.has_value());
            QCOMPARE(*type, GameType::Portal2);
        }
        {
            QString content = QStringLiteral("\"GameInfo\" { game \"Unknown Mod\" FileSystem { SteamAppId 550 } }\n");
            auto parsed = GameInfoParser::parseFromString(content, Core::Path::FilesystemPath(QStringLiteral("C:/mock/gameinfo.txt")));
            QVERIFY(parsed.has_value());
            auto type = GameValidator::identifyGameType(*parsed);
            QVERIFY(type.has_value());
            QCOMPARE(*type, GameType::L4D2);
        }

        // 4. Loose substring matching (longer matching pattern must take precedence)
        {
            QString content = QStringLiteral("\"GameInfo\" { game \"Portal 2 Thinking With Time Travel\" }\n");
            auto parsed = GameInfoParser::parseFromString(content, Core::Path::FilesystemPath(QStringLiteral("C:/mock/gameinfo.txt")));
            QVERIFY(parsed.has_value());
            auto type = GameValidator::identifyGameType(*parsed);
            QVERIFY(type.has_value());
            QCOMPARE(*type, GameType::Portal2);
            QVERIFY(!GameValidator::validateGameInfo(*parsed, GameType::Portal));
        }
        {
            QString content = QStringLiteral("\"GameInfo\" { game \"Left 4 Dead 2 Custom Campaign\" }\n");
            auto parsed = GameInfoParser::parseFromString(content, Core::Path::FilesystemPath(QStringLiteral("C:/mock/gameinfo.txt")));
            QVERIFY(parsed.has_value());
            auto type = GameValidator::identifyGameType(*parsed);
            QVERIFY(type.has_value());
            QCOMPARE(*type, GameType::L4D2);
            QVERIFY(!GameValidator::validateGameInfo(*parsed, GameType::L4D));
        }
    }

    void testCs2GameInfoParser() {
        QString cs2GiContent = QStringLiteral(
            "\"GameInfo\"\n"
            "{\n"
            "    game \"Counter-Strike 2\"\n"
            "    title \"Counter-Strike 2\"\n"
            "    LayeredOnMod csgo_imported\n"
            "    FileSystem\n"
            "    {\n"
            "        SearchPaths\n"
            "        {\n"
            "            Game_LowViolence csgo_lv\n"
            "            Game csgo\n"
            "            Game csgo_imported\n"
            "            Game csgo_core\n"
            "            Game core\n"
            "            Mod csgo\n"
            "            Mod csgo_imported\n"
            "            Mod csgo_core\n"
            "            AddonRoot csgo_addons\n"
            "        }\n"
            "    }\n"
            "}\n"
        );

        Core::Path::FilesystemPath mockGiPath(QStringLiteral("C:/Games/SteamLibrary/steamapps/common/Counter-Strike Global Offensive/game/csgo/gameinfo.gi"));
        QString error;
        auto result = GameInfoParser::parseFromString(cs2GiContent, mockGiPath, EngineType::Source2, &error);
        QVERIFY2(result.has_value(), qPrintable(error));

        const auto& info = *result;
        QCOMPARE(info.game(), QStringLiteral("Counter-Strike 2"));
        QCOMPARE(info.title(), QStringLiteral("Counter-Strike 2"));
        QCOMPARE(info.modDirectory().toString(), QStringLiteral("C:/Games/SteamLibrary/steamapps/common/Counter-Strike Global Offensive/game/csgo"));
        QCOMPARE(info.baseDirectory().toString(), QStringLiteral("C:/Games/SteamLibrary/steamapps/common/Counter-Strike Global Offensive"));

        QVERIFY(GameValidator::validateGameInfo(info, GameType::CS2));
        QVERIFY(!GameValidator::validateGameInfo(info, GameType::CSS));

        auto identified = GameValidator::identifyGameType(info);
        QVERIFY(identified.has_value());
        QCOMPARE(*identified, GameType::CS2);
    }

    void testSource2GameDefinitionHelpers() {
        const auto* cs2Def = GameRegistry::findByType(GameType::CS2);
        QVERIFY(cs2Def != nullptr);
        QVERIFY(cs2Def->isSource2());
        QCOMPARE(cs2Def->modName(), QStringLiteral("csgo"));
        QCOMPARE(cs2Def->contentSubdirectory(), QStringLiteral("content/csgo"));
        QCOMPARE(cs2Def->addonModSubdirectory(), QStringLiteral("game/csgo_addons"));
        QCOMPARE(cs2Def->addonContentSubdirectory(), QStringLiteral("content/csgo_addons"));

        // Custom Source 2 definition
        GameDefinition mockS2Def;
        mockS2Def.engine = EngineType::Source2;
        mockS2Def.modSubdirectory = QStringLiteral("game/custom_mod");
        mockS2Def.gameInfoFileName = QStringLiteral("gameinfo.gi");
        QVERIFY(mockS2Def.isSource2());
        QCOMPARE(mockS2Def.modName(), QStringLiteral("custom_mod"));
        QCOMPARE(mockS2Def.contentSubdirectory(), QStringLiteral("content/custom_mod"));
        QCOMPARE(mockS2Def.addonModSubdirectory(), QStringLiteral("game/custom_mod_addons"));
        QCOMPARE(mockS2Def.addonContentSubdirectory(), QStringLiteral("content/custom_mod_addons"));
    }
};

QTEST_MAIN(TestGameInfo)
#include "TestGameInfo.moc"

