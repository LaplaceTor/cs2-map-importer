#include <QTest>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include "Application/Environment/SteamService.h"
#include "Application/Environment/GameDetectService.h"
#include "Application/Environment/GameInstallationValidator.h"
#include "Application/Environment/GameEnvironmentService.h"
#include "Application/Environment/GameInstallation.h"
#include "Domain/Game/GameType.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameInstallationResolver.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Path/PathUtils.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/Async/TaskResult.h"

using namespace Application::Environment;
using namespace Domain::Game;
using namespace Core::Async;

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
        auto cssValidation = GameInstallationValidator::validateGameDirectory(GameType::CSS, Core::Path::FilesystemPath(cssDirStr));
        QVERIFY(cssValidation.isSuccess());
        QCOMPARE(cssValidation.value().type(), GameType::CSS);
        QCOMPARE(cssValidation.value().gameTitle(), QStringLiteral("Counter-Strike Source"));
        QVERIFY(!cssValidation.value().isSource2());

        // Half-Life 2 fixture
        QString hl2DirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Half-Life 2"));
        auto hl2Validation = GameInstallationValidator::validateGameDirectory(GameType::HL2, Core::Path::FilesystemPath(hl2DirStr));
        QVERIFY(hl2Validation.isSuccess());
        QCOMPARE(hl2Validation.value().type(), GameType::HL2);
        QCOMPARE(hl2Validation.value().gameTitle(), QStringLiteral("HALF-LIFE 2"));

        // Left 4 Dead 2 fixture
        QString l4d2DirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Left 4 Dead 2"));
        auto l4d2Validation = GameInstallationValidator::validateGameDirectory(GameType::L4D2, Core::Path::FilesystemPath(l4d2DirStr));
        QVERIFY(l4d2Validation.isSuccess());
        QCOMPARE(l4d2Validation.value().gameTitle(), QStringLiteral("Left 4 Dead 2"));

        // Portal 2 fixture
        QString portal2DirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Portal 2"));
        auto portal2Validation = GameInstallationValidator::validateGameDirectory(GameType::Portal2, Core::Path::FilesystemPath(portal2DirStr));
        QVERIFY(portal2Validation.isSuccess());
        QCOMPARE(portal2Validation.value().gameTitle(), QStringLiteral("PORTAL 2"));

        // Team Fortress 2 fixture
        QString tf2DirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Team Fortress 2"));
        auto tf2Validation = GameInstallationValidator::validateGameDirectory(GameType::TF2, Core::Path::FilesystemPath(tf2DirStr));
        QVERIFY(tf2Validation.isSuccess());
        QCOMPARE(tf2Validation.value().gameTitle(), QStringLiteral("Team Fortress 2"));

        // CS:GO legacy fixture
        QString csgoDirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("csgo legacy"));
        auto csgoValidation = GameInstallationValidator::validateGameDirectory(GameType::CSGO, Core::Path::FilesystemPath(csgoDirStr));
        QVERIFY(csgoValidation.isSuccess());
        QCOMPARE(csgoValidation.value().gameTitle(), QStringLiteral("Counter-Strike: Global Offensive"));

        // Custom inspection
        QString customGiPath = QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source/cstrike/gameinfo.txt"));
        auto customValidation = GameInstallationValidator::inspectGameInfo(Core::Path::FilesystemPath(customGiPath));
        QVERIFY(customValidation.isSuccess());
        QCOMPARE(customValidation.value().type(), GameType::CSS);
        QCOMPARE(customValidation.value().gameTitle(), QStringLiteral("Counter-Strike Source"));
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
        auto validated = GameInstallationValidator::validateSource2(rootPath);
        QVERIFY(validated.isSuccess());
        QCOMPARE(validated.value().type(), GameType::CS2);
        QCOMPARE(validated.value().gameTitle(), QStringLiteral("Counter-Strike 2"));
        QVERIFY(validated.value().isSource2());
        QCOMPARE(validated.value().gameInfoPath().toString(), Core::Path::PathUtils::normalize(giFilePath));
        QCOMPARE(validated.value().baseDirectory().toString(), Core::Path::PathUtils::normalize(tempDir.path()));

        // Also test selecting gameinfo.gi directly
        auto directValidation = GameInstallationValidator::validateSource2(Core::Path::FilesystemPath(giFilePath));
        QVERIFY(directValidation.isSuccess());
        QCOMPARE(directValidation.value().gameTitle(), QStringLiteral("Counter-Strike 2"));
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
        ResolvedGameInstallation res;
        res.type = GameType::CS2;
        res.isSource2 = true;
        res.baseDirectory = Core::Path::FilesystemPath(QStringLiteral("C:/Games/CS2"));

        QCOMPARE(res.modName(), QStringLiteral("csgo"));
        QCOMPARE(res.contentDirectory().toString(), QStringLiteral("C:/Games/CS2/content/csgo"));
        QCOMPARE(res.modDirectory().toString(), QStringLiteral("C:/Games/CS2/game/csgo"));
        QCOMPARE(res.addonGameDirectory(QStringLiteral("my_map")).toString(), QStringLiteral("C:/Games/CS2/game/csgo_addons/my_map"));
        QCOMPARE(res.addonContentDirectory(QStringLiteral("my_map")).toString(), QStringLiteral("C:/Games/CS2/content/csgo_addons/my_map"));
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
        auto resolved = GameInstallationResolver::resolveSource2(rootPath);
        QVERIFY(resolved.has_value());
        QCOMPARE(resolved->gameInfo.game(), QStringLiteral("Future Source 2 Game"));
        QVERIFY(resolved->isSource2);
        QCOMPARE(resolved->modName(), QStringLiteral("future_game"));
        QCOMPARE(resolved->addonGameDirectory(QStringLiteral("test_addon")).toString(),
                 Core::Path::PathUtils::normalize(tempDir.filePath(QStringLiteral("game/future_game_addons/test_addon"))));
        QCOMPARE(resolved->addonContentDirectory(QStringLiteral("test_addon")).toString(),
                 Core::Path::PathUtils::normalize(tempDir.filePath(QStringLiteral("content/future_game_addons/test_addon"))));

        // Test inspectGameInfo on the root directory
        auto inspected = GameInstallationValidator::inspectGameInfo(rootPath);
        QVERIFY(inspected.isSuccess());
        QCOMPARE(inspected.value().gameTitle(), QStringLiteral("Future Source 2 Game"));
        QVERIFY(inspected.value().isSource2());
    }

    void testDetectEnvironmentResult() {
        auto result = GameDetectService::detectEnvironment();
        QVERIFY(result.isSuccess());
        // Result must be valid, installations can be empty or populated depending on host
        QVERIFY(result.value().installations.size() >= 0);
        // detectAllGames should return matching count
        auto allGames = GameDetectService::detectAllGames();
        QCOMPARE(allGames.size(), result.value().installations.size());
    }

    void testDetectEnvironmentAsync() {
        bool finished = false;
        GameDetectService::detectEnvironmentAsync(this, [&](const TaskResult<DetectionResult>& result) {
            finished = true;
            QVERIFY(result.isSuccess());
            QVERIFY(result.value().installations.size() >= 0);
        });

        QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);
    }

    void testGameInstallationValidatorDirect() {
        QString cssDirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source"));
        auto cssValidation = GameInstallationValidator::validateSource1(GameType::CSS, Core::Path::FilesystemPath(cssDirStr));
        QVERIFY(cssValidation.isSuccess());
        QCOMPARE(cssValidation.value().type(), GameType::CSS);
        QCOMPARE(cssValidation.value().gameTitle(), QStringLiteral("Counter-Strike Source"));

        QString customGiPath = QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source/cstrike/gameinfo.txt"));
        auto customValidation = GameInstallationValidator::inspectGameInfo(Core::Path::FilesystemPath(customGiPath));
        QVERIFY(customValidation.isSuccess());
        QCOMPARE(customValidation.value().type(), GameType::CSS);
    }

    void testGameInstallationResolver() {
        QString cssDirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source"));
        auto cssRes = GameInstallationResolver::resolveSource1(GameType::CSS, Core::Path::FilesystemPath(cssDirStr));
        QVERIFY(cssRes.has_value());
        QCOMPARE(cssRes->type, GameType::CSS);
        QVERIFY(cssRes->isValid);
        QVERIFY(!cssRes->isSource2);
        QCOMPARE(cssRes->gameInfo.game(), QStringLiteral("Counter-Strike Source"));

        QString customGiPath = QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source/cstrike/gameinfo.txt"));
        auto customRes = GameInstallationResolver::inspectGameInfo(Core::Path::FilesystemPath(customGiPath));
        QVERIFY(customRes.has_value());
        QCOMPARE(customRes->type, GameType::CSS);
    }

    void testGameEnvironmentServiceFacade() {
        GameEnvironmentService envService;
        QCOMPARE(envService.s1GameTypes().contains(QStringLiteral("CSGO")), true);
        QCOMPARE(envService.s2GameTypes().contains(QStringLiteral("Counter-Strike 2")), true);

        QString cssDirStr = QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source"));
        auto cssRes = envService.validateSource1Folder(QStringLiteral("CS: Source"), cssDirStr);
        QVERIFY(cssRes.isSuccess());
        QCOMPARE(cssRes.value().gameTitle, QStringLiteral("Counter-Strike Source"));

        bool asyncFinished = false;
        envService.validateSource1FolderAsync(QStringLiteral("CS: Source"), cssDirStr, this, [&](const TaskResult<GameInstallationInfo>& res) {
            asyncFinished = true;
            QVERIFY(res.isSuccess());
            QCOMPARE(res.value().gameTitle, QStringLiteral("Counter-Strike Source"));
        });
        QTRY_VERIFY_WITH_TIMEOUT(asyncFinished, 5000);
    }

    void testGameInstallationResolverAddons() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString addonDir = tempDir.filePath(QStringLiteral("game/csgo_addons/de_dust2_sub"));
        QVERIFY(QDir().mkpath(addonDir));

        auto addons = GameInstallationResolver::listSource2Addons(Core::Path::FilesystemPath(tempDir.path()));
        QCOMPARE(addons.size(), 1);
        QCOMPARE(addons.first(), QStringLiteral("de_dust2_sub"));

        GameEnvironmentService envService;
        auto facadeAddons = envService.listSource2Addons(tempDir.path());
        QCOMPARE(facadeAddons.size(), 1);
        QCOMPARE(facadeAddons.first(), QStringLiteral("de_dust2_sub"));
    }

    void testFacadeLeaseEncapsulation() {
        Application::Environment::VpkSignatureLeaseService leaseService;
        GameEnvironmentService envService(&leaseService);

        QVERIFY(!envService.isVpkLeaseHeld());
        QCOMPARE(envService.vpkLeaseStatus(), Application::Environment::VpkSignatureLeaseStatus::Inactive);
    }
};

QTEST_MAIN(TestEnvironment)
#include "TestEnvironment.moc"
