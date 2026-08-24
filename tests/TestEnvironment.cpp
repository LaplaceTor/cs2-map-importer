#include <QTest>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include "Application/Environment/SteamService.h"
#include "Application/Environment/GameDetectService.h"
#include "Application/Environment/GameInstallation.h"
#include "Domain/Game/GameType.h"
#include "Domain/Game/GameRegistry.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Path/PathUtils.h"
#include "Core/FileSystem/FileSystem.h"

using namespace Application::Environment;
using namespace Domain::Game;

class TestEnvironment : public QObject {
    Q_OBJECT

private:
    QString m_testFilesRoot;

private slots:
    void initTestCase() {
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

    void testSteamRegistryDetection() {
        auto steamPath = SteamService::detectSteamInstallPath();
#ifdef Q_OS_WIN
        // On Windows systems where Steam is installed, this must find a valid directory
        if (steamPath.isValid()) {
            QVERIFY(steamPath.exists());
            QVERIFY(steamPath.isDirectory());
        }
#endif
    }

    void testSteamLibraryDetection() {
        auto libraries = SteamService::detectLibraries();
        if (libraries.empty()) {
            QSKIP("No Steam libraries detected on this system, skipping live library tests.");
        }

        for (const auto& lib : libraries) {
            QVERIFY(lib.path.isValid());
            QVERIFY(lib.path.exists());
            QVERIFY(lib.path.isDirectory());
        }
    }

    void testHostCs2Detection() {
        auto optCs2 = GameDetectService::detectGame(GameType::CS2);
        if (!optCs2.has_value()) {
            QSKIP("Counter-Strike 2 is not installed on this host, skipping live CS2 test.");
        }

        const auto& cs2 = *optCs2;
        QCOMPARE(cs2.type(), GameType::CS2);
        QCOMPARE(cs2.gameId(), QStringLiteral("cs2"));
        QCOMPARE(cs2.gameTitle(), QStringLiteral("Counter-Strike 2"));
        QVERIFY(cs2.isSource2());
        QVERIFY(cs2.isValid());
        QVERIFY(cs2.baseDirectory().exists());
        QVERIFY(cs2.gameInfoPath().exists());
        QCOMPARE(cs2.gameInfoPath().fileName(), QStringLiteral("gameinfo.gi"));
    }

    void testHostSource1GamesDetection() {
        auto allGames = GameDetectService::detectAllGames();
        if (allGames.empty()) {
            QSKIP("No Steam games detected on this host, skipping live games test.");
        }

        for (const auto& game : allGames) {
            QVERIFY(game.isValid());
            QVERIFY(!game.gameTitle().isEmpty());
            QVERIFY(game.baseDirectory().exists());
            QVERIFY(game.gameInfoPath().exists());

            if (game.type() == GameType::CS2) {
                QCOMPARE(game.gameTitle(), QStringLiteral("Counter-Strike 2"));
                QVERIFY(game.isSource2());
            } else if (game.type() == GameType::CSS) {
                QCOMPARE(game.gameTitle(), QStringLiteral("Counter-Strike Source"));
                QVERIFY(!game.isSource2());
            } else if (game.type() == GameType::HL2) {
                QCOMPARE(game.gameTitle(), QStringLiteral("HALF-LIFE 2"));
                QVERIFY(!game.isSource2());
            } else if (game.type() == GameType::L4D2) {
                QCOMPARE(game.gameTitle(), QStringLiteral("Left 4 Dead 2"));
                QVERIFY(!game.isSource2());
            } else if (game.type() == GameType::Portal2) {
                QCOMPARE(game.gameTitle(), QStringLiteral("PORTAL 2"));
                QVERIFY(!game.isSource2());
            } else if (game.type() == GameType::TF2) {
                QCOMPARE(game.gameTitle(), QStringLiteral("Team Fortress 2"));
                QVERIFY(!game.isSource2());
            } else if (game.type() == GameType::CSGO) {
                QCOMPARE(game.gameTitle(), QStringLiteral("Counter-Strike: Global Offensive"));
                QVERIFY(!game.isSource2());
            }
        }
    }

    void testHostCssDetection() {
        auto optCss = GameDetectService::detectGame(GameType::CSS);
        if (!optCss.has_value()) {
            QSKIP("Counter-Strike: Source is not installed on this host, skipping.");
        }
        QCOMPARE(optCss->type(), GameType::CSS);
        QCOMPARE(optCss->gameTitle(), QStringLiteral("Counter-Strike Source"));
        QVERIFY(!optCss->isSource2());
        QVERIFY(optCss->baseDirectory().exists());
        QVERIFY(optCss->gameInfoPath().exists());
    }

    void testHostHl2Detection() {
        auto optHl2 = GameDetectService::detectGame(GameType::HL2);
        if (!optHl2.has_value()) {
            QSKIP("Half-Life 2 is not installed on this host, skipping.");
        }
        QCOMPARE(optHl2->type(), GameType::HL2);
        QCOMPARE(optHl2->gameTitle(), QStringLiteral("HALF-LIFE 2"));
        QVERIFY(!optHl2->isSource2());
        QVERIFY(optHl2->baseDirectory().exists());
        QVERIFY(optHl2->gameInfoPath().exists());
    }

    void testHostL4d2Detection() {
        auto optL4d2 = GameDetectService::detectGame(GameType::L4D2);
        if (!optL4d2.has_value()) {
            QSKIP("Left 4 Dead 2 is not installed on this host, skipping.");
        }
        QCOMPARE(optL4d2->type(), GameType::L4D2);
        QCOMPARE(optL4d2->gameTitle(), QStringLiteral("Left 4 Dead 2"));
        QVERIFY(!optL4d2->isSource2());
        QVERIFY(optL4d2->baseDirectory().exists());
        QVERIFY(optL4d2->gameInfoPath().exists());
    }

    void testHostPortal2Detection() {
        auto optPortal2 = GameDetectService::detectGame(GameType::Portal2);
        if (!optPortal2.has_value()) {
            QSKIP("Portal 2 is not installed on this host, skipping.");
        }
        QCOMPARE(optPortal2->type(), GameType::Portal2);
        QCOMPARE(optPortal2->gameTitle(), QStringLiteral("PORTAL 2"));
        QVERIFY(!optPortal2->isSource2());
        QVERIFY(optPortal2->baseDirectory().exists());
        QVERIFY(optPortal2->gameInfoPath().exists());
    }

    void testHostTf2Detection() {
        auto optTf2 = GameDetectService::detectGame(GameType::TF2);
        if (!optTf2.has_value()) {
            QSKIP("Team Fortress 2 is not installed on this host, skipping.");
        }
        QCOMPARE(optTf2->type(), GameType::TF2);
        QCOMPARE(optTf2->gameTitle(), QStringLiteral("Team Fortress 2"));
        QVERIFY(!optTf2->isSource2());
        QVERIFY(optTf2->baseDirectory().exists());
        QVERIFY(optTf2->gameInfoPath().exists());
    }

    void testHostCsgoLegacyDetection() {
        auto optCsgo = GameDetectService::detectGame(GameType::CSGO);
        if (!optCsgo.has_value()) {
            QSKIP("CS:GO / CS:GO Legacy is not installed on this host, skipping.");
        }
        QCOMPARE(optCsgo->type(), GameType::CSGO);
        QCOMPARE(optCsgo->gameTitle(), QStringLiteral("Counter-Strike: Global Offensive"));
        QVERIFY(!optCsgo->isSource2());
        QVERIFY(optCsgo->baseDirectory().exists());
        QVERIFY(optCsgo->gameInfoPath().exists());
    }

    void testValidateDirectoryWithFixtures() {
        // Counter-Strike Source fixture
        QString cssDirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source"));
        auto cssValidation = GameDetectService::validateGameDirectory(GameType::CSS, Core::Path::FilesystemPath(cssDirStr));
        QVERIFY(cssValidation.has_value());
        QCOMPARE(cssValidation->type(), GameType::CSS);
        QCOMPARE(cssValidation->gameTitle(), QStringLiteral("Counter-Strike Source"));
        QVERIFY(!cssValidation->isSource2());

        // Half-Life 2 fixture
        QString hl2DirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Half-Life 2"));
        auto hl2Validation = GameDetectService::validateGameDirectory(GameType::HL2, Core::Path::FilesystemPath(hl2DirStr));
        QVERIFY(hl2Validation.has_value());
        QCOMPARE(hl2Validation->type(), GameType::HL2);
        QCOMPARE(hl2Validation->gameTitle(), QStringLiteral("HALF-LIFE 2"));

        // Left 4 Dead 2 fixture
        QString l4d2DirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Left 4 Dead 2"));
        auto l4d2Validation = GameDetectService::validateGameDirectory(GameType::L4D2, Core::Path::FilesystemPath(l4d2DirStr));
        QVERIFY(l4d2Validation.has_value());
        QCOMPARE(l4d2Validation->gameTitle(), QStringLiteral("Left 4 Dead 2"));

        // Portal 2 fixture
        QString portal2DirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Portal 2"));
        auto portal2Validation = GameDetectService::validateGameDirectory(GameType::Portal2, Core::Path::FilesystemPath(portal2DirStr));
        QVERIFY(portal2Validation.has_value());
        QCOMPARE(portal2Validation->gameTitle(), QStringLiteral("PORTAL 2"));

        // Team Fortress 2 fixture
        QString tf2DirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Team Fortress 2"));
        auto tf2Validation = GameDetectService::validateGameDirectory(GameType::TF2, Core::Path::FilesystemPath(tf2DirStr));
        QVERIFY(tf2Validation.has_value());
        QCOMPARE(tf2Validation->gameTitle(), QStringLiteral("Team Fortress 2"));

        // CS:GO legacy fixture
        QString csgoDirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("csgo legacy"));
        auto csgoValidation = GameDetectService::validateGameDirectory(GameType::CSGO, Core::Path::FilesystemPath(csgoDirStr));
        QVERIFY(csgoValidation.has_value());
        QCOMPARE(csgoValidation->gameTitle(), QStringLiteral("Counter-Strike: Global Offensive"));

        // Custom inspection
        QString customGiPath = QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source/cstrike/gameinfo.txt"));
        auto customValidation = GameDetectService::inspectGameInfo(Core::Path::FilesystemPath(customGiPath));
        QVERIFY(customValidation.has_value());
        QCOMPARE(customValidation->type(), GameType::CSS);
        QCOMPARE(customValidation->gameTitle(), QStringLiteral("Counter-Strike Source"));
    }

    void testMockCs2Installation() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString csgoModDir = tempDir.filePath(QStringLiteral("game/csgo"));
        QVERIFY(QDir().mkpath(csgoModDir));

        QString giFilePath = QDir(csgoModDir).filePath(QStringLiteral("gameinfo.gi"));
        QFile giFile(giFilePath);
        QVERIFY(giFile.open(QIODevice::WriteOnly | QIODevice::Text));
        giFile.write(
            "\"GameInfo\"\n"
            "{\n"
            "    game \"Counter-Strike 2\"\n"
            "    title \"Counter-Strike 2\"\n"
            "    FileSystem\n"
            "    {\n"
            "        SearchPaths\n"
            "        {\n"
            "            Game csgo\n"
            "        }\n"
            "    }\n"
            "}\n"
        );
        giFile.close();

        Core::Path::FilesystemPath rootPath(tempDir.path());
        auto validated = GameDetectService::validateSource2(rootPath);
        QVERIFY(validated.has_value());
        QCOMPARE(validated->type(), GameType::CS2);
        QCOMPARE(validated->gameTitle(), QStringLiteral("Counter-Strike 2"));
        QVERIFY(validated->isSource2());
        QCOMPARE(validated->gameInfoPath().toString(), Core::Path::PathUtils::normalize(giFilePath));
        QCOMPARE(validated->baseDirectory().toString(), Core::Path::PathUtils::normalize(tempDir.path()));

        // Also test selecting gameinfo.gi directly
        auto directValidation = GameDetectService::validateSource2(Core::Path::FilesystemPath(giFilePath));
        QVERIFY(directValidation.has_value());
        QCOMPARE(directValidation->gameTitle(), QStringLiteral("Counter-Strike 2"));
    }

    void testMockSteamLibraryFolders() {
        QTemporaryDir tempSteamDir;
        QVERIFY(tempSteamDir.isValid());

        QString steamappsDir = tempSteamDir.filePath(QStringLiteral("steamapps"));
        QVERIFY(QDir().mkpath(steamappsDir));

        QString vdfPath = QDir(steamappsDir).filePath(QStringLiteral("libraryfolders.vdf"));
        QFile vdfFile(vdfPath);
        QVERIFY(vdfFile.open(QIODevice::WriteOnly | QIODevice::Text));
        vdfFile.write(
            "\"libraryfolders\"\n"
            "{\n"
            "    \"0\"\n"
            "    {\n"
            "        \"path\" \"" + tempSteamDir.path().toUtf8().replace('/', "\\\\") + "\"\n"
            "        \"apps\"\n"
            "        {\n"
            "            \"730\" \"1000\"\n"
            "            \"240\" \"2000\"\n"
            "        }\n"
            "    }\n"
            "}\n"
        );
        vdfFile.close();

        auto libraries = SteamService::parseLibraryFolders(Core::Path::FilesystemPath(vdfPath));
        QCOMPARE(libraries.size(), static_cast<size_t>(1));
        QCOMPARE(libraries[0].path.toString(), Core::Path::PathUtils::normalize(tempSteamDir.path()));
        QCOMPARE(libraries[0].installedAppIds.size(), static_cast<size_t>(2));
        QCOMPARE(libraries[0].installedAppIds[0], 730);
        QCOMPARE(libraries[0].installedAppIds[1], 240);
    }

    void testGameInstallationSource2Paths() {
        GameInstallation inst;
        inst.setType(GameType::CS2);
        inst.setSource2(true);
        inst.setBaseDirectory(Core::Path::FilesystemPath(QStringLiteral("C:/Games/CS2")));

        QCOMPARE(inst.modName(), QStringLiteral("csgo"));
        QCOMPARE(inst.contentDirectory().toString(), QStringLiteral("C:/Games/CS2/content/csgo"));
        QCOMPARE(inst.modDirectory().toString(), QStringLiteral("C:/Games/CS2/game/csgo"));
        QCOMPARE(inst.addonGameDirectory(QStringLiteral("my_map")).toString(), QStringLiteral("C:/Games/CS2/game/csgo_addons/my_map"));
        QCOMPARE(inst.addonContentDirectory(QStringLiteral("my_map")).toString(), QStringLiteral("C:/Games/CS2/content/csgo_addons/my_map"));
    }

    void testMockCustomSource2Installation() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString customModDir = tempDir.filePath(QStringLiteral("game/future_game"));
        QVERIFY(QDir().mkpath(customModDir));

        QString giFilePath = QDir(customModDir).filePath(QStringLiteral("gameinfo.gi"));
        QFile giFile(giFilePath);
        QVERIFY(giFile.open(QIODevice::WriteOnly | QIODevice::Text));
        giFile.write(
            "\"GameInfo\"\n"
            "{\n"
            "    game \"Future Source 2 Game\"\n"
            "    title \"Future Source 2 Game\"\n"
            "    FileSystem\n"
            "    {\n"
            "        SearchPaths\n"
            "        {\n"
            "            Game future_game\n"
            "        }\n"
            "    }\n"
            "}\n"
        );
        giFile.close();

        Core::Path::FilesystemPath rootPath(tempDir.path());

        // Validate generic Source 2 directory without specifying game type
        auto validated = GameDetectService::validateSource2(rootPath);
        QVERIFY(validated.has_value());
        QCOMPARE(validated->gameTitle(), QStringLiteral("Future Source 2 Game"));
        QVERIFY(validated->isSource2());
        QCOMPARE(validated->modName(), QStringLiteral("future_game"));
        QCOMPARE(validated->addonGameDirectory(QStringLiteral("test_addon")).toString(),
                 Core::Path::PathUtils::normalize(tempDir.filePath(QStringLiteral("game/future_game_addons/test_addon"))));
        QCOMPARE(validated->addonContentDirectory(QStringLiteral("test_addon")).toString(),
                 Core::Path::PathUtils::normalize(tempDir.filePath(QStringLiteral("content/future_game_addons/test_addon"))));

        // Test inspectGameInfo on the root directory
        auto inspected = GameDetectService::inspectGameInfo(rootPath);
        QVERIFY(inspected.has_value());
        QCOMPARE(inspected->gameTitle(), QStringLiteral("Future Source 2 Game"));
        QVERIFY(inspected->isSource2());
    }
};

QTEST_MAIN(TestEnvironment)
#include "TestEnvironment.moc"
