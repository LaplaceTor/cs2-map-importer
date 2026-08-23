#include <QTest>
#include <QDir>
#include <QFileInfo>
#include "Domain/Game/GameInfo.h"
#include "Domain/Game/GameInfoParser.h"
#include "Domain/Game/SearchPathResolver.h"
#include "Domain/Game/SearchTarget.h"
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
};

QTEST_MAIN(TestGameInfo)
#include "TestGameInfo.moc"

