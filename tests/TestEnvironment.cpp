#include <QTest>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include "Application/Environment/SteamService.h"
#include "Application/Environment/Internal/SteamLibraryDetector.h"
#include "Application/Environment/GameDetectService.h"
#include "Application/Environment/GameInstallationValidator.h"
#include "Application/Environment/GameEnvironmentService.h"
#include "Application/Environment/GameInstallation.h"
#include "Domain/Game/GameType.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameInstallationResolver.h"
#include "Domain/Game/SteamGameLocator.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Path/PathUtils.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/Error/Exception.h"
#include "Core/Result/Result.h"
#include "Application/Execution/ExecutionGuard.h"

using namespace Application::Environment;
using namespace Application::Environment::Internal;
using namespace Domain::Game;
using Core::Result;
using Core::ResultStatus;

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
        auto steamRes = SteamLibraryDetector::detectSteamInstallPath();
        // On Windows systems where Steam is installed, this must find a valid directory
        if (steamRes.isSuccess()) {
            QVERIFY(steamRes.value().isValid());
            QVERIFY(steamRes.value().exists());
            QVERIFY(steamRes.value().isDirectory());
        } else {
            QCOMPARE(steamRes.errorCode(), Core::Error::ErrorCode::DirectoryNotFound);
        }
    }

    void testSteamLibraryDetection() {
        auto libRes = SteamLibraryDetector::detectLibraries();
        if (!libRes.isSuccess()) {
            QSKIP("No Steam libraries detected on this system, skipping live library tests.");
        }

        const auto& libraries = libRes.value();
        for (const auto& lib : libraries) {
            QVERIFY(lib.path.isValid());
            QVERIFY(lib.path.exists());
            QVERIFY(lib.path.isDirectory());
        }
    }

    void testHostCs2Detection() {
        auto cs2Res = GameDetectService::detectGame(GameType::CS2);
        if (!cs2Res.isSuccess()) {
            QSKIP("Counter-Strike 2 is not installed on this host, skipping live CS2 test.");
        }

        const auto& cs2 = cs2Res.value();
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
        auto envRes = GameDetectService::detectEnvironment();
        QVERIFY(envRes.isSuccess());
        const auto& allGames = envRes.value().installations;
        if (allGames.empty()) {
            QSKIP("No Steam games detected on this host, skipping live games test.");
        }

        for (const auto& game : allGames) {
            QVERIFY(game.isValid);
            QVERIFY(!game.displayName.isEmpty());
            QVERIFY(!game.gameId.isEmpty());
            QVERIFY(QDir(game.basePath).exists());
            QVERIFY(QFile::exists(game.gameInfoPath));
        }
    }

    void testHostCssDetection() {
        auto cssRes = GameDetectService::detectGame(GameType::CSS);
        if (!cssRes.isSuccess()) {
            QSKIP("Counter-Strike: Source is not installed on this host, skipping.");
        }
        QCOMPARE(cssRes.value().type(), GameType::CSS);
        QCOMPARE(cssRes.value().gameTitle(), QStringLiteral("Counter-Strike Source"));
        QVERIFY(!cssRes.value().isSource2());
        QVERIFY(cssRes.value().baseDirectory().exists());
        QVERIFY(cssRes.value().gameInfoPath().exists());
    }

    void testHostHl2Detection() {
        auto hl2Res = GameDetectService::detectGame(GameType::HL2);
        if (!hl2Res.isSuccess()) {
            QSKIP("Half-Life 2 is not installed on this host, skipping.");
        }
        QCOMPARE(hl2Res.value().type(), GameType::HL2);
        QCOMPARE(hl2Res.value().gameTitle(), QStringLiteral("HALF-LIFE 2"));
        QVERIFY(!hl2Res.value().isSource2());
        QVERIFY(hl2Res.value().baseDirectory().exists());
        QVERIFY(hl2Res.value().gameInfoPath().exists());
    }

    void testHostL4d2Detection() {
        auto l4d2Res = GameDetectService::detectGame(GameType::L4D2);
        if (!l4d2Res.isSuccess()) {
            QSKIP("Left 4 Dead 2 is not installed on this host, skipping.");
        }
        QCOMPARE(l4d2Res.value().type(), GameType::L4D2);
        QCOMPARE(l4d2Res.value().gameTitle(), QStringLiteral("Left 4 Dead 2"));
        QVERIFY(!l4d2Res.value().isSource2());
        QVERIFY(l4d2Res.value().baseDirectory().exists());
        QVERIFY(l4d2Res.value().gameInfoPath().exists());
    }

    void testHostPortal2Detection() {
        auto portal2Res = GameDetectService::detectGame(GameType::Portal2);
        if (!portal2Res.isSuccess()) {
            QSKIP("Portal 2 is not installed on this host, skipping.");
        }
        QCOMPARE(portal2Res.value().type(), GameType::Portal2);
        QCOMPARE(portal2Res.value().gameTitle(), QStringLiteral("PORTAL 2"));
        QVERIFY(!portal2Res.value().isSource2());
        QVERIFY(portal2Res.value().baseDirectory().exists());
        QVERIFY(portal2Res.value().gameInfoPath().exists());
    }

    void testHostTf2Detection() {
        auto tf2Res = GameDetectService::detectGame(GameType::TF2);
        if (!tf2Res.isSuccess()) {
            QSKIP("Team Fortress 2 is not installed on this host, skipping.");
        }
        QCOMPARE(tf2Res.value().type(), GameType::TF2);
        QCOMPARE(tf2Res.value().gameTitle(), QStringLiteral("Team Fortress 2"));
        QVERIFY(!tf2Res.value().isSource2());
        QVERIFY(tf2Res.value().baseDirectory().exists());
        QVERIFY(tf2Res.value().gameInfoPath().exists());
    }

    void testHostCsgoLegacyDetection() {
        auto csgoRes = GameDetectService::detectGame(GameType::CSGO);
        if (!csgoRes.isSuccess()) {
            QSKIP("CS:GO / CS:GO Legacy is not installed on this host, skipping.");
        }
        QCOMPARE(csgoRes.value().type(), GameType::CSGO);
        QCOMPARE(csgoRes.value().gameTitle(), QStringLiteral("Counter-Strike: Global Offensive"));
        QVERIFY(!csgoRes.value().isSource2());
        QVERIFY(csgoRes.value().baseDirectory().exists());
        QVERIFY(csgoRes.value().gameInfoPath().exists());
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

        auto libRes = SteamLibraryDetector::parseLibraryFolders(Core::Path::FilesystemPath(vdfPath));
        QVERIFY(libRes.isSuccess());
        const auto& libraries = libRes.value();
        QCOMPARE(libraries.size(), static_cast<size_t>(1));
        QCOMPARE(libraries[0].path.toString(), Core::Path::PathUtils::normalize(tempSteamDir.path()));
        QCOMPARE(libraries[0].installedAppIds.size(), static_cast<size_t>(2));
        QCOMPARE(libraries[0].installedAppIds[0], 730);
        QCOMPARE(libraries[0].installedAppIds[1], 240);
    }

    void testCorruptedLibraryFoldersVdf() {
        QTemporaryDir tempSteamDir;
        QVERIFY(tempSteamDir.isValid());

        QString steamappsDir = tempSteamDir.filePath(QStringLiteral("steamapps"));
        QVERIFY(QDir().mkpath(steamappsDir));

        QString vdfPath = QDir(steamappsDir).filePath(QStringLiteral("libraryfolders.vdf"));
        QFile vdfFile(vdfPath);
        QVERIFY(vdfFile.open(QIODevice::WriteOnly | QIODevice::Text));
        // Write corrupted / invalid syntax VDF
        vdfFile.write("\"libraryfolders\" { \"unclosed_quote_and_syntax_error");
        vdfFile.close();

        // 1. parseLibraryFolders directly on corrupted file must return Failure
        auto parseRes = SteamLibraryDetector::parseLibraryFolders(Core::Path::FilesystemPath(vdfPath));
        QVERIFY(parseRes.isFailure());
        QCOMPARE(parseRes.message(), QStringLiteral("Failed to parse libraryfolders.vdf"));

        // 2. detectLibraries on directory containing corrupted VDF must NOT silently fall back, but fail
        auto detectRes = SteamLibraryDetector::detectLibraries(Core::Path::FilesystemPath(tempSteamDir.path()));
        QVERIFY(detectRes.isFailure());
        QCOMPARE(detectRes.message(), QStringLiteral("Failed to parse Steam library configuration"));

        // 3. detectEnvironment with custom path containing corrupted VDF must return fatal Failure with configuration parse error (not "Invalid custom Steam path")
        auto envRes = GameDetectService::detectEnvironment(tempSteamDir.path());
        QVERIFY(envRes.isFailure());
        QCOMPARE(envRes.message(), QStringLiteral("Failed to parse Steam library configuration"));
        QVERIFY(envRes.errorCode() != Core::Error::ErrorCode::Success);

        // 3b. In contrast, non-existent custom path must return "Invalid custom Steam path"
        auto nonExistentEnvRes = GameDetectService::detectEnvironment(QStringLiteral("Z:/NonExistent/SteamDirectory12345"));
        QVERIFY(nonExistentEnvRes.isFailure());
        QCOMPARE(nonExistentEnvRes.message(), QStringLiteral("Invalid custom Steam path"));

        // 4. In contrast, when libraryfolders.vdf does not exist at all, detectLibraries falls back to root
        QVERIFY(QFile::remove(vdfPath));
        auto fallbackRes = SteamLibraryDetector::detectLibraries(Core::Path::FilesystemPath(tempSteamDir.path()));
        QVERIFY(fallbackRes.isSuccess());
        QCOMPARE(fallbackRes.value().size(), static_cast<size_t>(1));
        QCOMPARE(fallbackRes.value()[0].path.toString(), Core::Path::PathUtils::normalize(tempSteamDir.path()));

        // 5. Valid empty libraryfolders.vdf returns success with empty library list (no fallback)
        QVERIFY(vdfFile.open(QIODevice::WriteOnly | QIODevice::Text));
        vdfFile.write("\"libraryfolders\" { }\n");
        vdfFile.close();
        auto validEmptyRes = SteamLibraryDetector::detectLibraries(Core::Path::FilesystemPath(tempSteamDir.path()));
        QVERIFY(validEmptyRes.isSuccess());
        QVERIFY(validEmptyRes.value().empty());
    }

    void testManifestReaderDiagnosticRecording() {
        QTemporaryDir tempSteamDir;
        QVERIFY(tempSteamDir.isValid());

        QString steamappsDir = tempSteamDir.filePath(QStringLiteral("steamapps"));
        QVERIFY(QDir().mkpath(steamappsDir));

        // Create a mock CSS game directory inside steamapps/common/Counter-Strike Source
        QString commonDir = QDir(steamappsDir).filePath(QStringLiteral("common/Counter-Strike Source/cstrike"));
        QVERIFY(QDir().mkpath(commonDir));
        QString giPath = QDir(commonDir).filePath(QStringLiteral("gameinfo.txt"));
        QFile giFile(giPath);
        QVERIFY(giFile.open(QIODevice::WriteOnly | QIODevice::Text));
        giFile.write("\"GameInfo\" { game \"Counter-Strike Source\" title \"Counter-Strike Source\" FileSystem { SearchPaths { Game cstrike } } }");
        giFile.close();

        // 1. When manifest reader fails (e.g. invalid ACF), diagnosticHandler must receive a warning
        // but locator should still fall back to candidate heuristics and successfully resolve the game
        QString receivedWarning;
        auto results = SteamGameLocator::resolveGamesInLibrary(
            Core::Path::FilesystemPath(tempSteamDir.path()),
            {240}, // AppID 240 = CSS
            [](const Core::Path::FilesystemPath&, int) -> Core::Result<QString> {
                return Core::Result<QString>::failure(Core::Error::ErrorCode::InvalidFile, QStringLiteral("Corrupted ACF syntax"));
            },
            [&](const QString& warn) {
                receivedWarning = warn;
            });

        QVERIFY(!receivedWarning.isEmpty());
        QVERIFY(receivedWarning.contains(QStringLiteral("Corrupted ACF syntax")));
        QCOMPARE(results.size(), static_cast<size_t>(1));
        QCOMPARE(results[0].type, GameType::CSS);
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
    }

    void testDetectEnvironmentAsync() {
        bool finished = false;
        GameDetectService::detectEnvironmentAsync(this, [&](const Result<DetectionResult>& result) {
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
        envService.validateSource1FolderAsync(QStringLiteral("CS: Source"), cssDirStr, this, [&](const Result<GameInstallationInfo>& res) {
            asyncFinished = true;
            QVERIFY(res.isSuccess());
            QCOMPARE(res.value().gameTitle, QStringLiteral("Counter-Strike Source"));
        });
        QTRY_VERIFY_WITH_TIMEOUT(asyncFinished, 5000);

        // Failure case: invalid Source 1 directory must preserve operation summary
        auto failS1 = envService.validateSource1Folder(QStringLiteral("CS: Source"), m_testFilesRoot);
        QVERIFY(failS1.isFailure());
        QCOMPARE(failS1.message(), QStringLiteral("Source 1 validation failed"));
        QVERIFY(!failS1.error().message().isEmpty());

        bool asyncFailS1Finished = false;
        envService.validateSource1FolderAsync(QStringLiteral("CS: Source"), m_testFilesRoot, this, [&](const Result<GameInstallationInfo>& res) {
            asyncFailS1Finished = true;
            QVERIFY(res.isFailure());
            QCOMPARE(res.message(), QStringLiteral("Source 1 validation failed"));
            QVERIFY(!res.error().message().isEmpty());
        });
        QTRY_VERIFY_WITH_TIMEOUT(asyncFailS1Finished, 5000);

        // Failure case: invalid Source 2 directory must preserve operation summary
        auto failS2 = envService.validateSource2Folder(m_testFilesRoot);
        QVERIFY(failS2.isFailure());
        QCOMPARE(failS2.message(), QStringLiteral("Source 2 validation failed"));
        QVERIFY(!failS2.error().message().isEmpty());

        bool asyncFailS2Finished = false;
        envService.validateSource2FolderAsync(m_testFilesRoot, this, [&](const Result<GameInstallationInfo>& res) {
            asyncFailS2Finished = true;
            QVERIFY(res.isFailure());
            QCOMPARE(res.message(), QStringLiteral("Source 2 validation failed"));
            QVERIFY(!res.error().message().isEmpty());
        });
        QTRY_VERIFY_WITH_TIMEOUT(asyncFailS2Finished, 5000);
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

    void testExecutionGuardExceptionTranslation() {
        using namespace Application::Execution;

        // 1. Translating Core::Error::Exception preserves structured error and code
        auto res1 = ExecutionGuard::guard<int>([]() -> Result<int> {
            throw Core::Error::Exception(Core::Error::ErrorCode::FileNotFound, QStringLiteral("Missing required asset file"), QStringLiteral("C:/test/file.txt"));
        }, QStringLiteral("Asset load failed"));

        QVERIFY(res1.isFailure());
        QCOMPARE(res1.errorCode(), Core::Error::ErrorCode::FileNotFound);
        QCOMPARE(res1.message(), QStringLiteral("Asset load failed"));
        QCOMPARE(res1.error().message(), QStringLiteral("Missing required asset file"));
        QCOMPARE(res1.details(), QStringLiteral("C:/test/file.txt"));

        // 2. Translating std::runtime_error
        auto res2 = ExecutionGuard::guard<QString>([]() -> Result<QString> {
            throw std::runtime_error("Standard runtime exception occurred");
        }, QStringLiteral("Operation failed"));

        QVERIFY(res2.isFailure());
        QCOMPARE(res2.errorCode(), Core::Error::ErrorCode::Unknown);
        QCOMPARE(res2.message(), QStringLiteral("Operation failed"));
        QCOMPARE(res2.details(), QStringLiteral("Standard runtime exception occurred"));

        // 3. Translating unknown exception (catch (...))
        auto res3 = ExecutionGuard::guard<int>([]() -> Result<int> {
            throw int(42);
        }, QStringLiteral("Unknown crash avoided"));

        QVERIFY(res3.isFailure());
        QCOMPARE(res3.errorCode(), Core::Error::ErrorCode::Unknown);
        QCOMPARE(res3.message(), QStringLiteral("Unknown crash avoided"));

        // 4. Void overload
        auto res4 = ExecutionGuard::guard<void>([]() -> Result<void> {
            throw Core::Error::Exception(Core::Error::ErrorCode::PermissionDenied, QStringLiteral("Access denied"));
        }, QStringLiteral("Void operation failed"));

        QVERIFY(res4.isFailure());
        QCOMPARE(res4.errorCode(), Core::Error::ErrorCode::PermissionDenied);
        QCOMPARE(res4.message(), QStringLiteral("Void operation failed"));

        // 5. Automatic return type deduction
        auto res5 = ExecutionGuard::guard([]() -> Result<int> {
            return Result<int>::success(123);
        });
        QVERIFY(res5.isSuccess());
        QCOMPARE(res5.value(), 123);
    }

    void testSynchronousApiExceptionSafety() {
        GameEnvironmentService envService;

        // Synchronous calls with invalid or non-existent paths must return structured Failure without throwing
        auto resS1 = envService.validateSource1Folder(QStringLiteral("CSGO"), QStringLiteral("Z:/NonExistent/Directory/Path/That/Cannot/Exist"));
        QVERIFY(resS1.isFailure());
        QVERIFY(resS1.errorCode() != Core::Error::ErrorCode::Success);

        auto resS2 = envService.validateSource2Folder(QStringLiteral("Z:/NonExistent/Directory/Path/That/Cannot/Exist"));
        QVERIFY(resS2.isFailure());
        QVERIFY(resS2.errorCode() != Core::Error::ErrorCode::Success);

        auto resSteam = envService.validateGameInSteam(QStringLiteral("NonExistentGameType12345"));
        QVERIFY(resSteam.isFailure());

        auto resLease = envService.updateVpkLease(QStringLiteral("Z:/NonExistent/Path"));
        QVERIFY(resLease.isFailure());
    }

    void testApplicationHelperExceptionBubbling() {
        // 1. SteamLibraryDetector methods with invalid inputs return structured Result::failure with ErrorCode and details
        auto emptyLibs = SteamLibraryDetector::detectLibraries(Core::Path::FilesystemPath(QStringLiteral("Z:/NonExistent/SteamRoot")));
        QVERIFY(emptyLibs.isFailure());
        QCOMPARE(emptyLibs.errorCode(), Core::Error::ErrorCode::DirectoryNotFound);
        QVERIFY(!emptyLibs.details().isEmpty());

        auto emptyParse = SteamLibraryDetector::parseLibraryFolders(Core::Path::FilesystemPath(QStringLiteral("Z:/NonExistent/libraryfolders.vdf")));
        QVERIFY(emptyParse.isFailure());
        QCOMPARE(emptyParse.errorCode(), Core::Error::ErrorCode::FileNotFound);
        QVERIFY(!emptyParse.details().isEmpty());

        auto emptyAppDir = SteamLibraryDetector::readAppInstallDir(Core::Path::FilesystemPath(QStringLiteral("Z:/NonExistent/Lib")), 730);
        QVERIFY(emptyAppDir.isFailure());
        QCOMPARE(emptyAppDir.errorCode(), Core::Error::ErrorCode::DirectoryNotFound);

        auto emptyAppName = SteamLibraryDetector::readAppName(Core::Path::FilesystemPath(QStringLiteral("Z:/NonExistent/Lib")), 730);
        QVERIFY(emptyAppName.isFailure());
        QCOMPARE(emptyAppName.errorCode(), Core::Error::ErrorCode::DirectoryNotFound);

        // 2. Exception bubbling from helper to ExecutionGuard
        auto res = Application::Execution::ExecutionGuard::guard<int>([]() -> Core::Result<int> {
            // Helper throws Core::Error::Exception
            throw Core::Error::Exception(Core::Error::ErrorCode::InvalidFile, QStringLiteral("Corrupted library VDF"));
        }, QStringLiteral("Steam library discovery failed"));

        QVERIFY(res.isFailure());
        QCOMPARE(res.errorCode(), Core::Error::ErrorCode::InvalidFile);
        QCOMPARE(res.message(), QStringLiteral("Steam library discovery failed"));
        QCOMPARE(res.error().message(), QStringLiteral("Corrupted library VDF"));

        // 3. GameDetectService fatal error vs benign missing Steam
        // Explicit invalid custom Steam path must return Failure
        auto invalidCustomRes = GameDetectService::detectEnvironment(QStringLiteral("Z:/NonExistent/CustomSteam"));
        QVERIFY(invalidCustomRes.isFailure());
        QCOMPARE(invalidCustomRes.message(), QStringLiteral("Invalid custom Steam path"));

        // 4. ExecutionGuard type trait constraint verification
        static_assert(Core::is_core_result_v<Core::Result<int>>, "Result<int> must satisfy is_core_result_v");
        static_assert(Core::is_core_result_v<Core::Result<void>>, "Result<void> must satisfy is_core_result_v");
        static_assert(!Core::is_core_result_v<int>, "int must not satisfy is_core_result_v");
        static_assert(!Core::is_core_result_v<QString>, "QString must not satisfy is_core_result_v");
    }
};

QTEST_MAIN(TestEnvironment)
#include "TestEnvironment.moc"
