#include <QTest>
#include <QSignalSpy>
#include <QDir>
#include <QUrl>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickStyle>
#include <QThreadPool>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>

#include "Core/Logging/LogManager.h"

#include "UI/Controllers/MainController.h"
#include "UI/ViewModels/LogViewModel.h"
#include "Application/Particle/ParticleImportDTOs.h"
#include "Application/Particle/ParticleImportService.h"
#include "Workflow/Common/ImportContext.h"
#include "Workflow/Particle/ParticleImportOptions.h"
#include "Workflow/Particle/ParticleImportWorkflow.h"
#include "Core/Result/Result.h"
#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"

using namespace UI::Controllers;
using namespace UI::ViewModels;
using namespace Application::Particle;
using namespace Workflow::Particle;
using namespace Core::Error;

class MockGameViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList s1GameTypes READ s1GameTypes WRITE setS1GameTypes NOTIFY changed)
    Q_PROPERTY(QString selectedS1Type READ selectedS1Type WRITE setSelectedS1Type NOTIFY changed)
    Q_PROPERTY(QString s1GamePath READ s1GamePath WRITE setS1GamePath NOTIFY changed)
    Q_PROPERTY(QString s1GameTitle READ s1GameTitle WRITE setS1GameTitle NOTIFY changed)
    Q_PROPERTY(bool isS1Valid READ isS1Valid WRITE setIsS1Valid NOTIFY changed)

    Q_PROPERTY(QStringList s2GameTypes READ s2GameTypes WRITE setS2GameTypes NOTIFY changed)
    Q_PROPERTY(QString selectedS2Type READ selectedS2Type WRITE setSelectedS2Type NOTIFY changed)
    Q_PROPERTY(QString s2GamePath READ s2GamePath WRITE setS2GamePath NOTIFY changed)
    Q_PROPERTY(QString s2GameTitle READ s2GameTitle WRITE setS2GameTitle NOTIFY changed)
    Q_PROPERTY(bool isS2Valid READ isS2Valid WRITE setIsS2Valid NOTIFY changed)
    Q_PROPERTY(bool isDetecting READ isDetecting WRITE setIsDetecting NOTIFY changed)

    Q_PROPERTY(QStringList s2AddonsList READ s2AddonsList WRITE setS2AddonsList NOTIFY changed)
    Q_PROPERTY(QString selectedAddon READ selectedAddon WRITE setSelectedAddon NOTIFY changed)

public:
    explicit MockGameViewModel(QObject* parent = nullptr) : QObject(parent) {
        QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    }

    QStringList s1GameTypes() const { return m_s1GameTypes; }
    void setS1GameTypes(const QStringList& l) { m_s1GameTypes = l; emit changed(); }

    QString selectedS1Type() const { return m_selectedS1Type; }
    Q_INVOKABLE void setSelectedS1Type(const QString& t) { m_selectedS1Type = t; emit changed(); }

    QString s1GamePath() const { return m_s1GamePath; }
    void setS1GamePath(const QString& p) { m_s1GamePath = p; emit changed(); }

    QString s1GameTitle() const { return m_s1GameTitle; }
    void setS1GameTitle(const QString& t) { m_s1GameTitle = t; emit changed(); }

    bool isS1Valid() const { return m_isS1Valid; }
    void setIsS1Valid(bool v) { m_isS1Valid = v; emit changed(); }

    QStringList s2GameTypes() const { return m_s2GameTypes; }
    void setS2GameTypes(const QStringList& l) { m_s2GameTypes = l; emit changed(); }

    QString selectedS2Type() const { return m_selectedS2Type; }
    Q_INVOKABLE void setSelectedS2Type(const QString& t) { m_selectedS2Type = t; emit changed(); }

    QString s2GamePath() const { return m_s2GamePath; }
    void setS2GamePath(const QString& p) { m_s2GamePath = p; emit changed(); }

    QString s2GameTitle() const { return m_s2GameTitle; }
    void setS2GameTitle(const QString& t) { m_s2GameTitle = t; emit changed(); }

    bool isS2Valid() const { return m_isS2Valid; }
    void setIsS2Valid(bool v) { m_isS2Valid = v; emit changed(); }

    bool isDetecting() const { return m_isDetecting; }
    void setIsDetecting(bool d) { m_isDetecting = d; emit changed(); }

    QStringList s2AddonsList() const { return m_s2AddonsList; }
    void setS2AddonsList(const QStringList& l) { m_s2AddonsList = l; emit changed(); }

    QString selectedAddon() const { return m_selectedAddon; }
    Q_INVOKABLE void setSelectedAddon(const QString& a) { m_selectedAddon = a; emit changed(); }

signals:
    void changed();

private:
    QStringList m_s1GameTypes = {QStringLiteral("CSGO"), QStringLiteral("CS: Source")};
    QString m_selectedS1Type = QStringLiteral("CSGO");
    QString m_s1GamePath;
    QString m_s1GameTitle = QStringLiteral("Counter-Strike: Global Offensive");
    bool m_isS1Valid = false;

    QStringList m_s2GameTypes = {QStringLiteral("Counter-Strike 2")};
    QString m_selectedS2Type = QStringLiteral("Counter-Strike 2");
    QString m_s2GamePath;
    QString m_s2GameTitle = QStringLiteral("Counter-Strike 2");
    bool m_isS2Valid = false;
    bool m_isDetecting = false;

    QStringList m_s2AddonsList;
    QString m_selectedAddon = QStringLiteral("test");
};

class TestParticleUI : public QObject {
    Q_OBJECT

private:
    QString m_qmlFilePath;

    static void createFile(const QString& filePath, const QByteArray& content = "dummy pcf content") {
        QFileInfo fi(filePath);
        QDir().mkpath(fi.path());
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(content);
            file.close();
        }
    }

    static void setupValidMockDirs(QTemporaryDir& temp, QString& s1Dir, QString& cs2Dir, QString& pcfFile) {
        s1Dir = temp.filePath(QStringLiteral("s1_game"));
        cs2Dir = temp.filePath(QStringLiteral("cs2_game"));
        QDir().mkpath(s1Dir);
        QDir().mkpath(cs2Dir);
        pcfFile = temp.filePath(QStringLiteral("particles_sample.pcf"));
        createFile(pcfFile);
    }

private slots:
    void initTestCase() {
        QQuickStyle::setStyle(QStringLiteral("Fusion"));
        m_qmlFilePath = QDir(QStringLiteral(PROJECT_SOURCE_DIR)).filePath(QStringLiteral("src/qml/cs2importer/tabs/ParticleTab.qml"));
        QVERIFY2(QFile::exists(m_qmlFilePath), qPrintable(QStringLiteral("ParticleTab.qml not found at: %1").arg(m_qmlFilePath)));
    }

    void cleanupTestCase() {
        QThreadPool::globalInstance()->waitForDone(3000);
        QCoreApplication::processEvents();
        Core::Logging::LogManager::instance().clear();
    }

    void init() {
        Core::Logging::LogManager::instance().clear();
    }

    void cleanup() {
        QThreadPool::globalInstance()->waitForDone(3000);
        QCoreApplication::processEvents();
        Core::Logging::LogManager::instance().clear();
    }

    void testQmlParticleTabExplicitIdsAndAliases() {
        QQmlEngine engine;
        engine.addImportPath(QDir(QStringLiteral(PROJECT_SOURCE_DIR)).filePath(QStringLiteral("src/qml")));

        QQmlComponent component(&engine, QUrl::fromLocalFile(m_qmlFilePath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        QScopedPointer<QObject> root(component.create());
        QVERIFY2(!root.isNull(), qPrintable(component.errorString()));

        // Verify explicit child objects by objectName
        auto* allowDepthBlendCheck = root->findChild<QObject*>(QStringLiteral("allowDepthBlendCheck"));
        QVERIFY(allowDepthBlendCheck != nullptr);

        auto* disableDiffuseCheck = root->findChild<QObject*>(QStringLiteral("disableDiffuseCheck"));
        QVERIFY(disableDiffuseCheck != nullptr);

        auto* addonCombo = root->findChild<QObject*>(QStringLiteral("addonCombo"));
        QVERIFY(addonCombo != nullptr);

        auto* startBtn = root->findChild<QObject*>(QStringLiteral("startBtn"));
        QVERIFY(startBtn != nullptr);

        auto* stopBtn = root->findChild<QObject*>(QStringLiteral("stopBtn"));
        QVERIFY(stopBtn != nullptr);

        // Verify initial states via root property aliases
        QCOMPARE(root->property("allowDepthBlend").toBool(), false);
        QCOMPARE(root->property("disableDiffuse").toBool(), false);
        QCOMPARE(allowDepthBlendCheck->property("checked").toBool(), false);
        QCOMPARE(disableDiffuseCheck->property("checked").toBool(), false);

        // Verify property alias mutability
        root->setProperty("allowDepthBlend", true);
        QCOMPARE(root->property("allowDepthBlend").toBool(), true);
        QCOMPARE(allowDepthBlendCheck->property("checked").toBool(), true);

        root->setProperty("disableDiffuse", true);
        QCOMPARE(root->property("disableDiffuse").toBool(), true);
        QCOMPARE(disableDiffuseCheck->property("checked").toBool(), true);

        // Toggle back
        root->setProperty("allowDepthBlend", false);
        QCOMPARE(root->property("allowDepthBlend").toBool(), false);
        QCOMPARE(allowDepthBlendCheck->property("checked").toBool(), false);
    }

    void testQmlParticleTabAddonModelFromViewModel() {
        QQmlEngine engine;
        engine.addImportPath(QDir(QStringLiteral(PROJECT_SOURCE_DIR)).filePath(QStringLiteral("src/qml")));

        QQmlComponent component(&engine, QUrl::fromLocalFile(m_qmlFilePath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        QScopedPointer<QObject> root(component.create());
        QVERIFY2(!root.isNull(), qPrintable(component.errorString()));

        auto* addonCombo = root->findChild<QObject*>(QStringLiteral("addonCombo"));
        QVERIFY(addonCombo != nullptr);

        // Scenario 1: gameViewModel is null -> model is empty
        QStringList modelWhenNull = addonCombo->property("model").toStringList();
        QVERIFY(modelWhenNull.isEmpty());

        // Scenario 2: gameViewModel has empty addons list -> model is empty
        MockGameViewModel mockVm;
        mockVm.setS2AddonsList({});
        root->setProperty("gameViewModel", QVariant::fromValue(&mockVm));

        QStringList modelWhenEmpty = addonCombo->property("model").toStringList();
        QVERIFY(modelWhenEmpty.isEmpty());

        // Scenario 3: gameViewModel has addons -> model directly matches s2AddonsList without test injection
        mockVm.setS2AddonsList({QStringLiteral("de_dust2_hd"), QStringLiteral("cs_office_v2")});
        QStringList modelWithAddons = addonCombo->property("model").toStringList();
        QCOMPARE(modelWithAddons, QStringList({QStringLiteral("de_dust2_hd"), QStringLiteral("cs_office_v2")}));
    }

    void testQmlParticleTabButtonBindings() {
        QQmlEngine engine;
        engine.addImportPath(QDir(QStringLiteral(PROJECT_SOURCE_DIR)).filePath(QStringLiteral("src/qml")));

        QQmlComponent component(&engine, QUrl::fromLocalFile(m_qmlFilePath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        QScopedPointer<QObject> root(component.create());
        QVERIFY2(!root.isNull(), qPrintable(component.errorString()));

        auto* startBtn = root->findChild<QObject*>(QStringLiteral("startBtn"));
        auto* stopBtn = root->findChild<QObject*>(QStringLiteral("stopBtn"));
        QVERIFY(startBtn != nullptr);
        QVERIFY(stopBtn != nullptr);

        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QString s1Dir, cs2Dir, pcfFile;
        setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

        MockGameViewModel mockVm;
        mockVm.setS1GamePath(s1Dir);
        mockVm.setS2GamePath(cs2Dir);

        MainController controller;
        root->setProperty("gameViewModel", QVariant::fromValue(&mockVm));
        root->setProperty("mainController", QVariant::fromValue(&controller));

        // Case 1: Games invalid, no PCF path -> START disabled, STOP disabled
        QCOMPARE(startBtn->property("enabled").toBool(), false);
        QCOMPARE(stopBtn->property("enabled").toBool(), false);

        // Case 2: Games valid, but no PCF path -> START still disabled
        mockVm.setIsS1Valid(true);
        mockVm.setIsS2Valid(true);
        QCOMPARE(startBtn->property("enabled").toBool(), false);

        // Case 3: Games valid AND PCF path set -> START enabled!
        root->setProperty("selectedPcfPath", pcfFile);
        QCOMPARE(startBtn->property("enabled").toBool(), true);
        QCOMPARE(stopBtn->property("enabled").toBool(), false);

        // Case 4: Import is processing -> START disabled, STOP enabled!
        auto importService = std::make_unique<ParticleImportService>();
        std::atomic<bool> holdTask{true};
        importService->setWorkflowRunner([&holdTask](const ParticleImportOptions&, const Workflow::Common::ImportContext&) {
            while (holdTask.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return Core::Result<ParticleImportWorkflowResult>::success({});
        });
        controller.setParticleImportService(std::move(importService));

        controller.startParticleImport(s1Dir, cs2Dir, QStringLiteral("test"), pcfFile, false, false);
        QVERIFY(controller.isProcessing());

        QCOMPARE(startBtn->property("enabled").toBool(), false);
        QCOMPARE(stopBtn->property("enabled").toBool(), true);

        // Release hold and wait for completion
        holdTask.store(false);
        QTRY_VERIFY(!controller.isProcessing());

        QCOMPARE(startBtn->property("enabled").toBool(), true);
        QCOMPARE(stopBtn->property("enabled").toBool(), false);
    }

    void testQmlParticleTabStartButtonInvocation() {
        QQmlEngine engine;
        engine.addImportPath(QDir(QStringLiteral(PROJECT_SOURCE_DIR)).filePath(QStringLiteral("src/qml")));

        QQmlComponent component(&engine, QUrl::fromLocalFile(m_qmlFilePath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        QScopedPointer<QObject> root(component.create());
        QVERIFY2(!root.isNull(), qPrintable(component.errorString()));

        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QString s1Dir, cs2Dir, pcfFile;
        setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

        MockGameViewModel mockVm;
        mockVm.setIsS1Valid(true);
        mockVm.setIsS2Valid(true);
        mockVm.setS1GamePath(s1Dir);
        mockVm.setS2GamePath(cs2Dir);
        mockVm.setSelectedS1Type(QStringLiteral("CSGO"));
        mockVm.setS2AddonsList({QStringLiteral("my_addon")});
        mockVm.setSelectedAddon(QStringLiteral("my_addon"));

        MainController controller;
        auto importService = std::make_unique<ParticleImportService>();
        ParticleImportOptions capturedOptions;
        std::atomic<bool> runnerCalled{false};

        importService->setWorkflowRunner([&capturedOptions, &runnerCalled](const ParticleImportOptions& opts, const Workflow::Common::ImportContext&) {
            capturedOptions = opts;
            runnerCalled.store(true);
            return Core::Result<ParticleImportWorkflowResult>::success({});
        });
        controller.setParticleImportService(std::move(importService));

        root->setProperty("gameViewModel", QVariant::fromValue(&mockVm));
        root->setProperty("mainController", QVariant::fromValue(&controller));
        root->setProperty("selectedPcfPath", pcfFile);
        root->setProperty("allowDepthBlend", true);
        root->setProperty("disableDiffuse", false);

        auto* startBtn = root->findChild<QObject*>(QStringLiteral("startBtn"));
        QVERIFY(startBtn != nullptr);
        QCOMPARE(startBtn->property("enabled").toBool(), true);

        // Click start button via clicked signal
        QVERIFY(QMetaObject::invokeMethod(startBtn, "clicked"));

        QTRY_VERIFY(runnerCalled.load());
        QTRY_VERIFY(!controller.isProcessing());

        QCOMPARE(capturedOptions.source1GameDir.toString(), s1Dir);
        QCOMPARE(capturedOptions.cs2BaseDir.toString(), cs2Dir);
        QCOMPARE(capturedOptions.addonName, QStringLiteral("my_addon"));
        QCOMPARE(capturedOptions.sourcePcfPath.toString(), pcfFile);
        QCOMPARE(capturedOptions.allowDepthBlend, true);
        QCOMPARE(capturedOptions.disableDiffuse, false);
        QCOMPARE(capturedOptions.isCsgo, true);
    }

    void testQmlParticleTabStopButtonInvocation() {
        QQmlEngine engine;
        engine.addImportPath(QDir(QStringLiteral(PROJECT_SOURCE_DIR)).filePath(QStringLiteral("src/qml")));

        QQmlComponent component(&engine, QUrl::fromLocalFile(m_qmlFilePath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        QScopedPointer<QObject> root(component.create());
        QVERIFY2(!root.isNull(), qPrintable(component.errorString()));

        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QString s1Dir, cs2Dir, pcfFile;
        setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

        MockGameViewModel mockVm;
        mockVm.setIsS1Valid(true);
        mockVm.setIsS2Valid(true);
        mockVm.setS1GamePath(s1Dir);
        mockVm.setS2GamePath(cs2Dir);
        mockVm.setSelectedS1Type(QStringLiteral("CSGO"));
        mockVm.setS2AddonsList({QStringLiteral("my_addon")});
        mockVm.setSelectedAddon(QStringLiteral("my_addon"));

        MainController controller;
        auto importService = std::make_unique<ParticleImportService>();
        std::atomic<bool> runnerStarted{false};

        importService->setWorkflowRunner([&runnerStarted](const ParticleImportOptions&, const Workflow::Common::ImportContext& ctx) {
            runnerStarted.store(true);
            while (!ctx.isCancelled()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return Core::Result<ParticleImportWorkflowResult>::cancelled(QStringLiteral("Aborted by user"));
        });
        controller.setParticleImportService(std::move(importService));

        root->setProperty("gameViewModel", QVariant::fromValue(&mockVm));
        root->setProperty("mainController", QVariant::fromValue(&controller));
        root->setProperty("selectedPcfPath", pcfFile);

        auto* startBtn = root->findChild<QObject*>(QStringLiteral("startBtn"));
        auto* stopBtn = root->findChild<QObject*>(QStringLiteral("stopBtn"));
        QVERIFY(startBtn != nullptr);
        QVERIFY(stopBtn != nullptr);

        // Click start to begin import
        QVERIFY(QMetaObject::invokeMethod(startBtn, "clicked"));
        QTRY_VERIFY(runnerStarted.load());
        QVERIFY(controller.isProcessing());
        QCOMPARE(stopBtn->property("enabled").toBool(), true);

        // Click stop button via clicked signal on stopBtn
        QVERIFY(QMetaObject::invokeMethod(stopBtn, "clicked"));

        // Wait for cancellation and completion
        QTRY_VERIFY(!controller.isProcessing());
        QCOMPARE(stopBtn->property("enabled").toBool(), false);
        QCOMPARE(startBtn->property("enabled").toBool(), true);
    }

    void testMainControllerUrlSanitization() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QString s1Dir, cs2Dir, pcfFile;
        setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

        MainController controller;
        auto importService = std::make_unique<ParticleImportService>();
        ParticleImportOptions capturedOpts;

        importService->setWorkflowRunner([&capturedOpts](const ParticleImportOptions& opts, const Workflow::Common::ImportContext&) {
            capturedOpts = opts;
            return Core::Result<ParticleImportWorkflowResult>::success({});
        });
        controller.setParticleImportService(std::move(importService));

        // Test with file:/// URL
        QString fileUrl = QStringLiteral("file:///") + pcfFile;
        controller.startParticleImport(
            s1Dir,
            cs2Dir,
            QStringLiteral("my_addon"),
            fileUrl,
            true,
            true,
            QStringLiteral("CSGO")
        );

        QTRY_VERIFY(!controller.isProcessing());
        QThreadPool::globalInstance()->waitForDone(2000);

        // Native path without file:///
        QCOMPARE(capturedOpts.sourcePcfPath.toString(), pcfFile);
        QCOMPARE(capturedOpts.addonName, QStringLiteral("my_addon"));
        QCOMPARE(capturedOpts.allowDepthBlend, true);
        QCOMPARE(capturedOpts.disableDiffuse, true);
        QCOMPARE(capturedOpts.isCsgo, true);

        // Test with file:// URL (2 slashes)
        QString fileUrl2 = QStringLiteral("file://") + pcfFile;
        controller.startParticleImport(
            s1Dir,
            cs2Dir,
            QStringLiteral("custom_addon"),
            fileUrl2,
            false,
            false,
            QStringLiteral("CSS")
        );

        QTRY_VERIFY(!controller.isProcessing());
        QThreadPool::globalInstance()->waitForDone(2000);

        QCOMPARE(capturedOpts.sourcePcfPath.toString(), pcfFile);
        QCOMPARE(capturedOpts.addonName, QStringLiteral("custom_addon"));
        QCOMPARE(capturedOpts.allowDepthBlend, false);
        QCOMPARE(capturedOpts.disableDiffuse, false);
        QCOMPARE(capturedOpts.isCsgo, false);

        // Test with plain local path
        controller.startParticleImport(
            s1Dir,
            cs2Dir,
            QStringLiteral("test"),
            pcfFile,
            false,
            false
        );

        QTRY_VERIFY(!controller.isProcessing());
        QThreadPool::globalInstance()->waitForDone(2000);
        QCOMPARE(capturedOpts.sourcePcfPath.toString(), pcfFile);
    }

    void testMainControllerIsProcessingFlagTransitions() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QString s1Dir, cs2Dir, pcfFile;
        setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

        MainController controller;
        auto importService = std::make_unique<ParticleImportService>();
        std::atomic<bool> holdTask{true};
        std::atomic<int> runCount{0};

        importService->setWorkflowRunner([&holdTask, &runCount](const ParticleImportOptions&, const Workflow::Common::ImportContext&) {
            runCount.fetch_add(1);
            while (holdTask.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return Core::Result<ParticleImportWorkflowResult>::success({});
        });
        controller.setParticleImportService(std::move(importService));

        QSignalSpy spyProcessing(&controller, &MainController::isProcessingChanged);

        QCOMPARE(controller.isProcessing(), false);

        controller.startParticleImport(
            s1Dir,
            cs2Dir,
            QStringLiteral("test"),
            pcfFile,
            false,
            false
        );

        // Immediate flag transition to true
        QCOMPARE(controller.isProcessing(), true);
        QVERIFY(spyProcessing.count() >= 1);

        // Wait until runner starts and is holding
        QTRY_COMPARE(runCount.load(), 1);

        // Re-entrant call while isProcessing is true must return early without incrementing runCount
        controller.startParticleImport(
            s1Dir,
            cs2Dir,
            QStringLiteral("test"),
            pcfFile,
            false,
            false
        );
        QCOMPARE(runCount.load(), 1);

        // Unblock worker
        holdTask.store(false);

        // Transition back to false
        QTRY_VERIFY(!controller.isProcessing());
        QVERIFY(spyProcessing.count() >= 2);
        QThreadPool::globalInstance()->waitForDone(3000);
    }

    void testMainControllerSuccessAlert() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QString s1Dir, cs2Dir, pcfFile;
        setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

        MainController controller;
        auto importService = std::make_unique<ParticleImportService>();

        importService->setWorkflowRunner([](const ParticleImportOptions&, const Workflow::Common::ImportContext&) {
            ParticleImportWorkflowResult r;
            r.totalConverted = 5;
            r.totalCompiled = 5;
            r.generatedVpcfFiles = {QStringLiteral("p1.vpcf"), QStringLiteral("p2.vpcf")};
            r.compiledVpcfCFiles = {QStringLiteral("p1.vpcf_c"), QStringLiteral("p2.vpcf_c")};
            return Core::Result<ParticleImportWorkflowResult>::success(r, QStringLiteral("Imported 5 particle systems successfully"));
        });
        controller.setParticleImportService(std::move(importService));

        QSignalSpy spyAlert(&controller, &MainController::alertRequested);

        controller.startParticleImport(
            s1Dir,
            cs2Dir,
            QStringLiteral("test"),
            pcfFile,
            false,
            false
        );

        QTRY_COMPARE(spyAlert.count(), 1);
        auto args = spyAlert.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("Particle Import Complete"));
        QVERIFY(args.at(1).toString().contains(QStringLiteral("Imported 5 particle systems")));
        QCOMPARE(controller.isProcessing(), false);
    }

    void testMainControllerFailureAlert() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QString s1Dir, cs2Dir, pcfFile;
        setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

        MainController controller;
        auto importService = std::make_unique<ParticleImportService>();

        importService->setWorkflowRunner([](const ParticleImportOptions&, const Workflow::Common::ImportContext&) {
            return Core::Result<ParticleImportWorkflowResult>::failure(
                ErrorCode::OperationFailed,
                QStringLiteral("ResourceCompiler returned non-zero exit code 1"),
                QStringLiteral("Error compiling particle asset at line 102")
            );
        });
        controller.setParticleImportService(std::move(importService));

        QSignalSpy spyAlert(&controller, &MainController::alertRequested);

        controller.startParticleImport(
            s1Dir,
            cs2Dir,
            QStringLiteral("test"),
            pcfFile,
            false,
            false
        );

        QTRY_COMPARE(spyAlert.count(), 1);
        auto args = spyAlert.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("Particle Import Failed"));
        QVERIFY(args.at(1).toString().contains(QStringLiteral("ResourceCompiler returned non-zero exit code 1")));
        QCOMPARE(controller.isProcessing(), false);
    }

    void testMainControllerStopImportTriggersCancellation() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QString s1Dir, cs2Dir, pcfFile;
        setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

        MainController controller;
        auto importService = std::make_unique<ParticleImportService>();
        std::atomic<bool> runnerStarted{false};

        importService->setWorkflowRunner([&runnerStarted](const ParticleImportOptions&, const Workflow::Common::ImportContext& ctx) {
            runnerStarted.store(true);
            while (!ctx.isCancelled()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return Core::Result<ParticleImportWorkflowResult>::cancelled(QStringLiteral("User aborted particle import"));
        });
        controller.setParticleImportService(std::move(importService));

        QSignalSpy spyAlert(&controller, &MainController::alertRequested);

        controller.startParticleImport(
            s1Dir,
            cs2Dir,
            QStringLiteral("test"),
            pcfFile,
            false,
            false
        );

        QTRY_VERIFY(runnerStarted.load());
        QVERIFY(controller.isProcessing());

        // Call stopImport()
        controller.stopImport();

        QTRY_COMPARE(spyAlert.count(), 1);
        auto args = spyAlert.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("Import Cancelled"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("Particle import was cancelled by user."));
        QCOMPARE(controller.isProcessing(), false);
    }

    void testMainControllerStartImportFallback() {
        MainController controller;
        QSignalSpy spyAlert(&controller, &MainController::alertRequested);

        controller.startImport();

        QCOMPARE(spyAlert.count(), 1);
        auto args = spyAlert.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("Feature in Development"));
        QCOMPARE(controller.isProcessing(), false);
    }

    void testMainControllerResetsLogViewModelOnStart() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QString s1Dir, cs2Dir, pcfFile;
        setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

        LogViewModel logVm;
        logVm.appendLog(QStringLiteral("Previous session log message"), 1);
        QCOMPARE(logVm.totalMessageCount(), 1);

        MainController controller(&logVm);
        auto importService = std::make_unique<ParticleImportService>();
        importService->setWorkflowRunner([](const ParticleImportOptions&, const Workflow::Common::ImportContext&) {
            return Core::Result<ParticleImportWorkflowResult>::success({});
        });
        controller.setParticleImportService(std::move(importService));

        controller.startParticleImport(
            s1Dir,
            cs2Dir,
            QStringLiteral("test"),
            pcfFile,
            false,
            false
        );

        // Verification: logViewModel view was reset
        QCOMPARE(logVm.totalMessageCount(), 0);
        QCOMPARE(logVm.taskCount(), 0);

        QTRY_VERIFY(!controller.isProcessing());
    }
};

QTEST_MAIN(TestParticleUI)
#include "TestParticleUI.moc"
