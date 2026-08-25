#include <QTest>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "UI/ViewModels/GameViewModel.h"
#include "UI/ViewModels/LogViewModel.h"
#include "UI/Controllers/MainController.h"
#include "Application/Environment/GameEnvironmentService.h"
#include "Application/Environment/VpkSignatureLeaseService.h"
#include "Domain/Game/GameType.h"

using namespace UI::ViewModels;
using namespace UI::Controllers;

class TestUiViewModels : public QObject {
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

    void testGameViewModelTypes() {
        GameViewModel vm;
        auto s1Types = vm.s1GameTypes();
        QVERIFY(!s1Types.isEmpty());
        QVERIFY(s1Types.contains(QStringLiteral("CSGO")));
        QVERIFY(s1Types.contains(QStringLiteral("CS: Source")));
        QVERIFY(s1Types.contains(QStringLiteral("Custom")));

        auto s2Types = vm.s2GameTypes();
        QVERIFY(!s2Types.isEmpty());
        QVERIFY(s2Types.contains(QStringLiteral("Counter-Strike 2")));
    }

    void testGameViewModelSource1Validation() {
        GameViewModel vm;
        QSignalSpy spyS1Path(&vm, &GameViewModel::s1GamePathChanged);
        QSignalSpy spyS1Valid(&vm, &GameViewModel::s1ValidityChanged);

        vm.setSelectedS1Type(QStringLiteral("CS: Source"));
        QCOMPARE(vm.selectedS1Type(), QStringLiteral("CS: Source"));

        QString cssDir = QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source"));
        vm.selectS1Folder(cssDir);

        QTRY_VERIFY(vm.isS1Valid());
        QCOMPARE(vm.s1GameTitle(), QStringLiteral("Counter-Strike Source"));
        QVERIFY(vm.s1Installation().isValid());
        QCOMPARE(vm.s1Installation().type(), Domain::Game::GameType::CSS);
        QVERIFY(spyS1Path.size() >= 1);
        QVERIFY(spyS1Valid.size() >= 1);
    }

    void testGameViewModelCustomSource1Selection() {
        GameViewModel vm;
        vm.setSelectedS1Type(QStringLiteral("Custom"));
        QCOMPARE(vm.selectedS1Type(), QStringLiteral("Custom"));

        QString cssGiPath = QDir(m_testFilesRoot).filePath(QStringLiteral("Counter-Strike Source/cstrike/gameinfo.txt"));
        vm.selectS1Folder(cssGiPath);

        QTRY_VERIFY(vm.isS1Valid());
        // Must remain "Custom" and not automatically jump to "CS: Source"
        QCOMPARE(vm.selectedS1Type(), QStringLiteral("Custom"));
        QCOMPARE(vm.s1GameTitle(), QStringLiteral("Counter-Strike Source"));
    }

    void testGameViewModelSource2Validation() {
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

        // Create a mock addon directory
        QString addonDir = tempDir.filePath(QStringLiteral("game/csgo_addons/de_sample"));
        QVERIFY(QDir().mkpath(addonDir));

        GameViewModel vm;
        vm.selectS2Folder(tempDir.path());

        QTRY_VERIFY(vm.isS2Valid());
        QCOMPARE(vm.s2GameTitle(), QStringLiteral("Counter-Strike 2"));
        QVERIFY(vm.s2Installation().isValid());
        QCOMPARE(vm.s2Installation().type(), Domain::Game::GameType::CS2);

        QVERIFY(vm.s2AddonsList().contains(QStringLiteral("de_sample")));
        QCOMPARE(vm.selectedAddon(), QStringLiteral("de_sample"));
    }

    void testLogViewModelFormatting() {
        LogViewModel logVm;
        QSignalSpy spyLog(&logVm, &LogViewModel::logTextChanged);

        QCOMPARE(logVm.lineCount(), 0);
        logVm.appendLog(QStringLiteral("Information entry"), 1); // Info
        logVm.appendLog(QStringLiteral("Warning: something missing"), 2); // Warning
        logVm.appendLog(QStringLiteral("ERROR: failed to load"), 3); // Error

        QCOMPARE(logVm.lineCount(), 3);
        QVERIFY(spyLog.size() >= 3);

        QString html = logVm.formattedLogText();
        QVERIFY(html.contains(QStringLiteral("color='#FF5252'"))); // Error red
        QVERIFY(html.contains(QStringLiteral("color='#FFD740'"))); // Warning yellow

        logVm.clear();
        QCOMPARE(logVm.lineCount(), 0);
    }

    void testMainControllerProperties() {
        MainController controller;
        QSignalSpy spyTab(&controller, &MainController::activeTabChanged);
        QSignalSpy spyTheme(&controller, &MainController::themeChanged);

        QCOMPARE(controller.activeTab(), 0);
        controller.setActiveTab(1);
        QCOMPARE(controller.activeTab(), 1);
        QCOMPARE(spyTab.size(), 1);

        QCOMPARE(controller.theme(), QStringLiteral("system"));
        controller.cycleTheme();
        QCOMPARE(controller.theme(), QStringLiteral("light"));
        controller.cycleTheme();
        QCOMPARE(controller.theme(), QStringLiteral("dark"));
        controller.cycleTheme();
        QCOMPARE(controller.theme(), QStringLiteral("system"));
        QCOMPARE(spyTheme.size(), 3);

        QVERIFY(!controller.appVersion().isEmpty());
    }

    void testGameViewModelAsyncAutoDetect() {
        GameViewModel vm;
        QSignalSpy spyDetecting(&vm, &GameViewModel::isDetectingChanged);
        QSignalSpy spyFinished(&vm, &GameViewModel::detectionFinished);

        QVERIFY(!vm.isDetecting());

        vm.autoDetect();
        QVERIFY(spyDetecting.size() >= 1);

        // Wait for asynchronous detection to finish
        QTRY_VERIFY_WITH_TIMEOUT(spyFinished.size() >= 1, 5000);
        QVERIFY(!vm.isDetecting());
    }

    void testGameViewModelVpkLeaseDelegation() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString csgoModDir = tempDir.filePath(QStringLiteral("game/csgo"));
        QVERIFY(QDir().mkpath(csgoModDir));

        QString win64Dir = tempDir.filePath(QStringLiteral("game/bin/win64"));
        QVERIFY(QDir().mkpath(win64Dir));

        QString sigPath = QDir(win64Dir).filePath(QStringLiteral("vpk.signatures"));
        QFile sigFile(sigPath);
        QVERIFY(sigFile.open(QIODevice::WriteOnly));
        sigFile.write("dummy signatures");
        sigFile.close();

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

        Application::Environment::VpkSignatureLeaseService leaseService;
        Application::Environment::GameEnvironmentService envService(&leaseService);
        GameViewModel vm(&envService);
        QSignalSpy spyLeaseState(&vm, &GameViewModel::vpkLeaseStateChanged);

        QVERIFY(!vm.isVpkLeaseHeld());
        vm.selectS2Folder(tempDir.path());

        QTRY_VERIFY(vm.isS2Valid());
        QVERIFY(vm.isVpkLeaseHeld());
        QVERIFY(leaseService.isLeaseHeld());
        QVERIFY(spyLeaseState.size() >= 1);

        // Deselecting or switching resets lease
        vm.setSelectedS2Type(QStringLiteral("other"));
        QVERIFY(!vm.isVpkLeaseHeld());
        QVERIFY(!leaseService.isLeaseHeld());
    }
};

QTEST_MAIN(TestUiViewModels)
#include "TestUiViewModels.moc"
