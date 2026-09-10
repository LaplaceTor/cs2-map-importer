#include <QTest>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QEventLoop>
#include <QTimer>
#include <thread>
#include <chrono>

#include "Application/Particle/ParticleImportDTOs.h"
#include "Application/Particle/ParticleImportService.h"
#include "Application/Common/ImportPrerequisiteService.h"
#include "Application/Environment/VpkSignatureLeaseService.h"
#include "Core/Async/CancellationToken.h"
#include "Workflow/Common/ImportContext.h"
#include "Workflow/Particle/ParticleImportOptions.h"
#include "Workflow/Particle/ParticleImportWorkflow.h"
#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Error/Exception.h"
#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Logging/TaskSnapshot.h"
#include "Core/Logging/TaskState.h"

using namespace Application::Particle;
using namespace Application::Environment;
using namespace Application::Common;
using namespace Workflow::Common;
using namespace Workflow::Particle;
using namespace Core::Error;
using namespace Core::Logging;

class TestParticleService : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 1. Input Validation Tests
    void testInputValidation_EmptySource1GameDir();
    void testInputValidation_NonExistentSource1GameDir();
    void testInputValidation_EmptyCs2BaseDir();
    void testInputValidation_NonExistentCs2BaseDir();
    void testInputValidation_EmptyAddonName();
    void testInputValidation_EmptySourcePcf();
    void testInputValidation_NonExistentSourcePcf();

    // 2. ExecutionGuard Boundary Exception Translation Tests
    void testExecutionGuard_CoreErrorExceptionTranslation();
    void testExecutionGuard_StandardExceptionTranslation();
    void testExecutionGuard_UnknownExceptionTranslation();

    // 3. Synchronous Execution & Result Mapping Tests
    void testSynchronousImport_SuccessResultMapping();
    void testSynchronousImport_WorkflowFailurePropagated();
    void testSynchronousImport_WorkflowCancelledPropagated();

    // 4. Asynchronous Execution via AsyncTaskRunner & Callback Verification
    void testAsynchronousImport_SuccessCallback();
    void testAsynchronousImport_CancellationViaCancelCurrentImport();
    void testAsynchronousImport_ConcurrentImportsSupported();

    // 5. Dual-Plane Lifecycle: TaskState vs Business Result
    void testDualPlaneLifecycle_SuccessStateTransition();
    void testDualPlaneLifecycle_FailureStateTransition();

    // 6. VpkSignatureLeaseService Coordination
    void testLeaseService_AcquiredWhenCs2DirValid();

private:
    static void createFile(const QString& filePath, const QByteArray& content = "dummy content") {
        QFileInfo fi(filePath);
        QDir().mkpath(fi.path());
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(content);
            file.close();
        }
    }

    static void setupValidMockDirs(QTemporaryDir& temp, QString& s1Dir, QString& cs2Dir, QString& pcfFile) {
        s1Dir = temp.filePath("s1_game");
        cs2Dir = temp.filePath("cs2_game");
        QDir().mkpath(s1Dir);
        QDir().mkpath(cs2Dir);
        pcfFile = temp.filePath("particles_sample.pcf");
        createFile(pcfFile);
    }

    static Core::Result<ParticleImportResult> runSync(
        ParticleImportService& service,
        const ParticleImportRequest& req,
        Core::Logging::TaskLoggingContext* loggingCtx = nullptr)
    {
        QEventLoop loop;
        Core::Result<ParticleImportResult> out;
        service.importParticlesAsync(req, loggingCtx, [&](const Core::Result<ParticleImportResult>& res) {
            out = res;
            loop.quit();
        });
        loop.exec();
        return out;
    }
};

void TestParticleService::initTestCase()
{
    LogManager::instance().clear();
}

void TestParticleService::cleanupTestCase()
{
    LogManager::instance().clear();
}

void TestParticleService::init()
{
    LogManager::instance().clear();
}

void TestParticleService::cleanup()
{
    LogManager::instance().clear();
}

// ---------------------------------------------------------------------------
// 1. Input Validation Tests
// ---------------------------------------------------------------------------

void TestParticleService::testInputValidation_EmptySource1GameDir()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = QString(); // empty
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    auto result = runSync(service, req);

    QVERIFY(result.isFailure());
    QCOMPARE(result.error().code(), ErrorCode::InvalidPath);
}

void TestParticleService::testInputValidation_NonExistentSource1GameDir()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = temp.filePath("non_existent_s1_dir");
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    auto result = runSync(service, req);

    QVERIFY(result.isFailure());
    QCOMPARE(result.error().code(), ErrorCode::FileNotFound);
}

void TestParticleService::testInputValidation_EmptyCs2BaseDir()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = QStringLiteral("   "); // whitespace / empty
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    auto result = runSync(service, req);

    QVERIFY(result.isFailure());
    QCOMPARE(result.error().code(), ErrorCode::InvalidPath);
}

void TestParticleService::testInputValidation_NonExistentCs2BaseDir()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = temp.filePath("non_existent_cs2_dir");
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    auto result = runSync(service, req);

    QVERIFY(result.isFailure());
    QCOMPARE(result.error().code(), ErrorCode::FileNotFound);
}

void TestParticleService::testInputValidation_EmptyAddonName()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("  \t ");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    auto result = runSync(service, req);

    QVERIFY(result.isFailure());
    QCOMPARE(result.error().code(), ErrorCode::InvalidArgument);
}

void TestParticleService::testInputValidation_EmptySourcePcf()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = QString();

    ParticleImportService service;
    auto result = runSync(service, req);

    QVERIFY(result.isFailure());
    QCOMPARE(result.error().code(), ErrorCode::InvalidPath);
}

void TestParticleService::testInputValidation_NonExistentSourcePcf()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = temp.filePath("missing_particles.pcf");

    ParticleImportService service;
    auto result = runSync(service, req);

    QVERIFY(result.isFailure());
    QCOMPARE(result.error().code(), ErrorCode::FileNotFound);
}

// ---------------------------------------------------------------------------
// 2. ExecutionGuard Boundary Exception Translation Tests
// ---------------------------------------------------------------------------

void TestParticleService::testExecutionGuard_CoreErrorExceptionTranslation()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    service.setWorkflowRunner([](const ParticleImportOptions&, const ImportContext&)
        -> Core::Result<ParticleImportWorkflowResult> {
        throw Exception(Error(ErrorCode::CorruptedData, QStringLiteral("Corrupted PCF binary stream"), QStringLiteral("Byte 0x10FF")));
    });

    auto result = runSync(service, req);

    QVERIFY(result.isFailure());
    QCOMPARE(result.error().code(), ErrorCode::CorruptedData);
    QCOMPARE(result.error().message(), QStringLiteral("Corrupted PCF binary stream"));
    QCOMPARE(result.details(), QStringLiteral("Byte 0x10FF"));
    QCOMPARE(result.message(), QStringLiteral("Particle import failed"));
}

void TestParticleService::testExecutionGuard_StandardExceptionTranslation()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    service.setWorkflowRunner([](const ParticleImportOptions&, const ImportContext&)
        -> Core::Result<ParticleImportWorkflowResult> {
        throw std::runtime_error("Disk I/O failure during extraction");
    });

    auto result = runSync(service, req);

    QVERIFY(result.isFailure());
    QCOMPARE(result.error().code(), ErrorCode::Unknown);
    QVERIFY(result.details().contains(QStringLiteral("Disk I/O failure during extraction")));
    QCOMPARE(result.message(), QStringLiteral("Particle import failed"));
}

void TestParticleService::testExecutionGuard_UnknownExceptionTranslation()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    service.setWorkflowRunner([](const ParticleImportOptions&, const ImportContext&)
        -> Core::Result<ParticleImportWorkflowResult> {
        throw 12345; // unknown non-std exception
    });

    auto result = runSync(service, req);

    QVERIFY(result.isFailure());
    QCOMPARE(result.error().code(), ErrorCode::Unknown);
    QCOMPARE(result.message(), QStringLiteral("Particle import failed"));
}

// ---------------------------------------------------------------------------
// 3. Synchronous Execution & Result Mapping Tests
// ---------------------------------------------------------------------------

void TestParticleService::testSynchronousImport_SuccessResultMapping()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;
    req.allowDepthBlend = true;
    req.disableDiffuse = true;
    req.isCsgo = true;

    ParticleImportService service;
    bool optionsVerified = false;
    service.setWorkflowRunner([&req, &optionsVerified](const ParticleImportOptions& opts, const ImportContext&)
        -> Core::Result<ParticleImportWorkflowResult> {
        if (opts.source1GameDir.toString() == req.source1GameDir &&
            opts.cs2BaseDir.toString() == req.cs2BaseDir &&
            opts.addonName == req.addonName &&
            opts.sourcePcfPath.toString() == req.sourcePcfPath &&
            opts.allowDepthBlend == req.allowDepthBlend &&
            opts.disableDiffuse == req.disableDiffuse &&
            opts.isCsgo == req.isCsgo)
        {
            optionsVerified = true;
        }

        ParticleImportWorkflowResult wfResult;
        wfResult.generatedVpcfFiles = {QStringLiteral("particles/explosion.vpcf"), QStringLiteral("particles/smoke.vpcf")};
        wfResult.compiledVpcfCFiles = {QStringLiteral("particles/explosion.vpcf_c"), QStringLiteral("particles/smoke.vpcf_c")};
        wfResult.totalConverted = 2;
        wfResult.totalCompiled = 2;
        return Core::Result<ParticleImportWorkflowResult>::success(wfResult, QStringLiteral("Workflow success"));
    });

    auto result = runSync(service, req);

    QVERIFY(optionsVerified);
    QVERIFY2(result.isSuccess(), qPrintable(result.message()));
    const auto& val = result.value();
    QVERIFY(val.succeeded);
    QCOMPARE(val.totalConverted, 2);
    QCOMPARE(val.totalCompiled, 2);
    QCOMPARE(val.generatedVpcfFiles.size(), 2);
    QCOMPARE(val.compiledVpcfCFiles.size(), 2);
    QCOMPARE(val.generatedVpcfFiles.at(0), QStringLiteral("particles/explosion.vpcf"));
    QCOMPARE(val.compiledVpcfCFiles.at(1), QStringLiteral("particles/smoke.vpcf_c"));
    QVERIFY(!service.isImporting());
}

void TestParticleService::testSynchronousImport_WorkflowFailurePropagated()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    service.setWorkflowRunner([](const ParticleImportOptions&, const ImportContext&)
        -> Core::Result<ParticleImportWorkflowResult> {
        return Core::Result<ParticleImportWorkflowResult>::failure(
            ErrorCode::ProcessFailed,
            QStringLiteral("source1import.exe exited with code 1"));
    });

    auto result = runSync(service, req);

    QVERIFY(result.isFailure());
    QCOMPARE(result.error().code(), ErrorCode::ProcessFailed);
    QVERIFY(result.message().contains(QStringLiteral("source1import.exe exited with code 1")));
    QVERIFY(!service.isImporting());
}

void TestParticleService::testSynchronousImport_WorkflowCancelledPropagated()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    QEventLoop loop;
    Core::Result<ParticleImportResult> result;
    auto handle = service.importParticlesAsync(req, nullptr, [&](const Core::Result<ParticleImportResult>& res) {
        result = res;
        loop.quit();
    });
    handle.cancel();
    loop.exec();

    QVERIFY(result.isCancelled());
    QVERIFY(!service.isImporting());
}

// ---------------------------------------------------------------------------
// 4. Asynchronous Execution via AsyncTaskRunner & Callback Verification
// ---------------------------------------------------------------------------

void TestParticleService::testAsynchronousImport_SuccessCallback()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    service.setWorkflowRunner([](const ParticleImportOptions&, const ImportContext& ctx)
        -> Core::Result<ParticleImportWorkflowResult> {
        ctx.info(QStringLiteral("Executing mock async particle workflow..."));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        ParticleImportWorkflowResult wfResult;
        wfResult.generatedVpcfFiles = {QStringLiteral("particles/fire.vpcf")};
        wfResult.compiledVpcfCFiles = {QStringLiteral("particles/fire.vpcf_c")};
        wfResult.totalConverted = 1;
        wfResult.totalCompiled = 1;
        return Core::Result<ParticleImportWorkflowResult>::success(wfResult, QStringLiteral("Async import completed"));
    });

    QEventLoop loop;
    Core::Result<ParticleImportResult> asyncOutcome;
    bool callbackReceived = false;

    auto handle = service.importParticlesAsync(req, nullptr, [&](const Core::Result<ParticleImportResult>& res) {
        asyncOutcome = res;
        callbackReceived = true;
        loop.quit();
    });

    QVERIFY(!handle.isCancelled());
    QVERIFY(service.isImporting());

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(callbackReceived);
    QVERIFY2(asyncOutcome.isSuccess(), qPrintable(asyncOutcome.message()));
    QCOMPARE(asyncOutcome.value().totalConverted, 1);
    QCOMPARE(asyncOutcome.value().totalCompiled, 1);
    QVERIFY(!service.isImporting());
}

void TestParticleService::testAsynchronousImport_CancellationViaCancelCurrentImport()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    service.setWorkflowRunner([](const ParticleImportOptions&, const ImportContext& ctx)
        -> Core::Result<ParticleImportWorkflowResult> {
        // Wait until cancellation signal arrives
        for (int i = 0; i < 100; ++i) {
            if (ctx.isCancelled()) {
                return Core::Result<ParticleImportWorkflowResult>::cancelled(QStringLiteral("Workflow detected cancellation"));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return Core::Result<ParticleImportWorkflowResult>::failure(
            ErrorCode::Timeout, QStringLiteral("Did not receive cancellation in time"));
    });

    QEventLoop loop;
    Core::Result<ParticleImportResult> asyncOutcome;
    bool callbackReceived = false;

    auto handle = service.importParticlesAsync(req, nullptr, [&](const Core::Result<ParticleImportResult>& res) {
        asyncOutcome = res;
        callbackReceived = true;
        loop.quit();
    });

    QVERIFY(service.isImporting());
    QVERIFY(handle.isValid());

    // Trigger cancellation via ParticleImportService::cancelCurrentImport()
    service.cancelCurrentImport();

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(callbackReceived);
    QVERIFY(asyncOutcome.isCancelled());
    QVERIFY(!service.isImporting());
}

void TestParticleService::testAsynchronousImport_ConcurrentImportsSupported()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    service.setWorkflowRunner([](const ParticleImportOptions&, const ImportContext&)
        -> Core::Result<ParticleImportWorkflowResult> {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ParticleImportWorkflowResult wf;
        wf.totalConverted = 1;
        wf.totalCompiled = 1;
        return Core::Result<ParticleImportWorkflowResult>::success(wf);
    });

    QEventLoop loop;
    int completedCount = 0;

    service.importParticlesAsync(req, nullptr, [&](const Core::Result<ParticleImportResult>& res1) {
        QVERIFY(res1.isSuccess());
        completedCount++;
        if (completedCount == 2) {
            loop.quit();
        }
    });

    service.importParticlesAsync(req, nullptr, [&](const Core::Result<ParticleImportResult>& res2) {
        QVERIFY(res2.isSuccess());
        completedCount++;
        if (completedCount == 2) {
            loop.quit();
        }
    });

    QVERIFY(service.isImporting());

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QCOMPARE(completedCount, 2);
    QVERIFY(!service.isImporting());
}

// ---------------------------------------------------------------------------
// 5. Dual-Plane Lifecycle: TaskState vs Business Result
// ---------------------------------------------------------------------------

void TestParticleService::testDualPlaneLifecycle_SuccessStateTransition()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    service.setWorkflowRunner([](const ParticleImportOptions&, const ImportContext& ctx)
        -> Core::Result<ParticleImportWorkflowResult> {
        ctx.info(QStringLiteral("Sub-task step 1"));
        ctx.updateProgress(0.5, QStringLiteral("Halfway"));
        ctx.info(QStringLiteral("Sub-task step 2"));
        ctx.updateProgress(1.0, QStringLiteral("Done"));
        ParticleImportWorkflowResult wf;
        wf.totalConverted = 1;
        wf.totalCompiled = 1;
        return Core::Result<ParticleImportWorkflowResult>::success(wf, QStringLiteral("Success outcome"));
    });

    auto rootTask = LogManager::instance().createTask(QStringLiteral("RootImportTask"));
    QVERIFY(rootTask != nullptr);
    rootTask->start();

    QEventLoop loop;
    Core::Result<ParticleImportResult> outcome;

    service.importParticlesAsync(req, rootTask.get(), [&](const Core::Result<ParticleImportResult>& res) {
        outcome = res;
        loop.quit();
    });

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    // Business plane check
    QVERIFY(outcome.isSuccess());

    // Task lifecycle plane check
    auto allSnapshots = LogManager::instance().taskSnapshots();
    bool foundChildTask = false;
    for (const auto& s : allSnapshots) {
        if (s.taskName.startsWith(QStringLiteral("Import Particle:"))) {
            foundChildTask = true;
            QCOMPARE(s.parentTaskId, rootTask->taskId());
            QCOMPARE(s.state, TaskState::Completed);
            break;
        }
    }
    QVERIFY(foundChildTask);
}

void TestParticleService::testDualPlaneLifecycle_FailureStateTransition()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    ParticleImportService service;
    service.setWorkflowRunner([](const ParticleImportOptions&, const ImportContext& ctx)
        -> Core::Result<ParticleImportWorkflowResult> {
        ctx.error(QStringLiteral("Fatal tool error detected"));
        return Core::Result<ParticleImportWorkflowResult>::failure(
            ErrorCode::OperationFailed, QStringLiteral("Domain conversion crashed"));
    });

    auto rootTask = LogManager::instance().createTask(QStringLiteral("RootImportTaskFailure"));
    QVERIFY(rootTask != nullptr);
    rootTask->start();

    QEventLoop loop;
    Core::Result<ParticleImportResult> outcome;

    service.importParticlesAsync(req, rootTask.get(), [&](const Core::Result<ParticleImportResult>& res) {
        outcome = res;
        loop.quit();
    });

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    // Business plane
    QVERIFY(outcome.isFailure());

    // Task lifecycle plane
    auto allSnapshots = LogManager::instance().taskSnapshots();
    bool foundFailedTask = false;
    for (const auto& s : allSnapshots) {
        if (s.taskName.startsWith(QStringLiteral("Import Particle:"))) {
            foundFailedTask = true;
            QCOMPARE(s.state, TaskState::Failed);
            break;
        }
    }
    QVERIFY(foundFailedTask);
}

// ---------------------------------------------------------------------------
// 6. VpkSignatureLeaseService Coordination
// ---------------------------------------------------------------------------

void TestParticleService::testLeaseService_AcquiredWhenCs2DirValid()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString s1Dir, cs2Dir, pcfFile;
    setupValidMockDirs(temp, s1Dir, cs2Dir, pcfFile);

    // Create mock vpk.signatures inside cs2Dir/game/bin/win64/
    QString signaturesPath = temp.filePath("cs2_game/game/bin/win64/vpk.signatures");
    createFile(signaturesPath, "MOCK_VPK_SIGNATURE_CONTENT");
    QVERIFY(QFile::exists(signaturesPath));

    auto leaseService = std::make_shared<VpkSignatureLeaseService>();
    QVERIFY(!leaseService->isLeaseHeld());

    auto prereqService = std::make_shared<ImportPrerequisiteService>(leaseService);
    ParticleImportService service(prereqService);
    QCOMPARE(service.prerequisiteService(), prereqService);
    QCOMPARE(service.prerequisiteService()->leaseService(), leaseService);

    service.setWorkflowRunner([](const ParticleImportOptions&, const ImportContext&)
        -> Core::Result<ParticleImportWorkflowResult> {
        ParticleImportWorkflowResult wf;
        return Core::Result<ParticleImportWorkflowResult>::success(wf);
    });

    ParticleImportRequest req;
    req.source1GameDir = s1Dir;
    req.cs2BaseDir = cs2Dir;
    req.addonName = QStringLiteral("test");
    req.sourcePcfPath = pcfFile;

    auto result = runSync(service, req);
    QVERIFY(result.isSuccess());

    // Verify lease was successfully acquired for the CS2 directory
    QVERIFY(leaseService->isLeaseHeld());
    QCOMPARE(leaseService->currentStatus(), VpkSignatureLeaseStatus::Acquired);
    QCOMPARE(QDir::cleanPath(leaseService->leasedFilePath()), QDir::cleanPath(signaturesPath));

    leaseService->releaseLease();
    QVERIFY(!leaseService->isLeaseHeld());
}

QTEST_MAIN(TestParticleService)
#include "TestParticleService.moc"
