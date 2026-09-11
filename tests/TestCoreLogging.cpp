#include <QTest>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QDir>
#include <QFileInfo>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

#include "Core/Logging/LogManager.h"
#include "Core/Logging/LogFileManager.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Logging/TaskState.h"
#include "Core/Logging/TaskFileSink.h"
#include "Core/Logging/ILogSink.h"
#include "Core/Result/Result.h"
#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Process/ProcessResult.h"
#include "Core/Process/ProcessRunner.h"
#include "Core/Process/ProcessOptions.h"
#include "Core/Async/CancellationToken.h"

using namespace Core::Logging;
using namespace Core::Error;
using namespace Core::Process;
using Core::Result;
using Core::ResultStatus;

/**
 * @brief In-memory mock log sink for verifying Core logging events without any UI/Application dependencies.
 */
class MockCoreLogSink : public ILogSink {
public:
    struct TaskRecord {
        quint64 taskId = 0;
        QString taskName;
        qint64 startTimestamp = 0;
        QString logFilePath;
        TaskState terminalState = TaskState::Pending;
        bool terminated = false;
        QStringList messages;
    };

    bool onTaskCreated(quint64 taskId, const QString& taskName, qint64 startTimestamp, const QString& logFilePath) override
    {
        QMutexLocker<QMutex> locker(&m_mutex);
        TaskRecord rec;
        rec.taskId = taskId;
        rec.taskName = taskName;
        rec.startTimestamp = startTimestamp;
        rec.logFilePath = logFilePath;
        m_tasks[taskId] = rec;
        m_createdTaskIds.append(taskId);
        return true;
    }

    void onTaskTerminated(quint64 taskId, TaskState state) override
    {
        QMutexLocker<QMutex> locker(&m_mutex);
        if (m_tasks.contains(taskId)) {
            m_tasks[taskId].terminalState = state;
            m_tasks[taskId].terminated = true;
        }
        m_terminatedTaskIds.append(taskId);
    }

    bool writeBlock(const LogBlock& block, const QString& taskName) override
    {
        Q_UNUSED(taskName);
        QMutexLocker<QMutex> locker(&m_mutex);
        for (const auto& entry : block.entries()) {
            m_allMessages.append(entry.message);
            if (m_tasks.contains(entry.taskId)) {
                m_tasks[entry.taskId].messages.append(entry.message);
            }
        }
        return true;
    }

    bool flush() override
    {
        return true;
    }

    int taskCount() const
    {
        QMutexLocker<QMutex> locker(&m_mutex);
        return static_cast<int>(m_createdTaskIds.size());
    }

    bool hasTask(quint64 taskId) const
    {
        QMutexLocker<QMutex> locker(&m_mutex);
        return m_tasks.contains(taskId);
    }

    TaskState taskState(quint64 taskId) const
    {
        QMutexLocker<QMutex> locker(&m_mutex);
        return m_tasks.value(taskId).terminalState;
    }

    QStringList taskMessages(quint64 taskId) const
    {
        QMutexLocker<QMutex> locker(&m_mutex);
        return m_tasks.value(taskId).messages;
    }

    QStringList allMessages() const
    {
        QMutexLocker<QMutex> locker(&m_mutex);
        return m_allMessages;
    }

    void clear()
    {
        QMutexLocker<QMutex> locker(&m_mutex);
        m_tasks.clear();
        m_createdTaskIds.clear();
        m_terminatedTaskIds.clear();
        m_allMessages.clear();
    }

private:
    mutable QMutex m_mutex;
    QHash<quint64, TaskRecord> m_tasks;
    QList<quint64> m_createdTaskIds;
    QList<quint64> m_terminatedTaskIds;
    QStringList m_allMessages;
};

class TestCoreLogging : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Core Task & Logging Context tests
    void testSingleTaskLifecycle();
    void testTaskFailure();
    void testTaskCancellation();
    void testTaskSkipped();
    void testConcurrentTasksIsolation();
    void testMultiBlockTask();
    void testLoggedErrorTracking();
    void testInvalidParentTaskRejection();

    // Workflow & Tool Task hierarchy tests
    void testWorkflowLogFolderAndToolFiles();
    void testToolTaskCreationAndStreaming();

    // Process Runner tests
    void testProcessRunnerStreamingAndCancellation();
    void testProcessResultToErrorMapping();

    // Result & Error tests
    void testStructuredResultPayload();
    void testResultOutcomes();
};

void TestCoreLogging::initTestCase()
{
}

void TestCoreLogging::cleanupTestCase()
{
}

void TestCoreLogging::init()
{
    LogManager::instance().clear();
}

void TestCoreLogging::cleanup()
{
    LogManager::instance().clear();
}

void TestCoreLogging::testSingleTaskLifecycle()
{
    auto mockSink = std::make_shared<MockCoreLogSink>();
    LogManager::instance().addSink(mockSink);

    QCOMPARE(mockSink->taskCount(), 0);

    // 1. Create task
    auto task = LogManager::instance().createTask(QStringLiteral("Single Task Test"));
    QVERIFY(task != nullptr);
    QVERIFY(task->taskId() > 0);
    QCOMPARE(task->taskName(), QStringLiteral("Single Task Test"));
    QCOMPARE(task->state(), TaskState::Pending);
    QCOMPARE(mockSink->taskCount(), 1);

    // 2. Start task
    QVERIFY(task->start());
    QCOMPARE(task->state(), TaskState::Running);

    // 3. Emit logs and flush
    task->info(QStringLiteral("Step 1: Reading VPK header"));
    task->debug(QStringLiteral("Debug: CRC32 validated"));
    task->updateProgress(0.5, QStringLiteral("Halfway done"));
    QCOMPARE(task->progress(), 0.5);

    LogManager::instance().flushTask(task->taskId());

    QStringList msgs = mockSink->taskMessages(task->taskId());
    QVERIFY(msgs.contains(QStringLiteral("Step 1: Reading VPK header")));
    QVERIFY(msgs.contains(QStringLiteral("Debug: CRC32 validated")));

    // 4. Complete task
    QVERIFY(LogManager::instance().finishTask(task->taskId(), QStringLiteral("All done")));
    QCOMPARE(task->state(), TaskState::Completed);
    QCOMPARE(task->progress(), 1.0);
    QCOMPARE(mockSink->taskState(task->taskId()), TaskState::Completed);

    LogManager::instance().removeSink(mockSink);
}

void TestCoreLogging::testTaskFailure()
{
    auto mockSink = std::make_shared<MockCoreLogSink>();
    LogManager::instance().addSink(mockSink);

    auto task = LogManager::instance().createTask(QStringLiteral("Failing Task"));
    task->start();
    task->error(QStringLiteral("Corrupt header encountered"));
    LogManager::instance().flushTask(task->taskId());

    QCOMPARE(task->errorCount(), 1);

    QVERIFY(LogManager::instance().failTask(task->taskId(), QStringLiteral("Task aborted")));
    QCOMPARE(task->state(), TaskState::Failed);
    QCOMPARE(mockSink->taskState(task->taskId()), TaskState::Failed);

    LogManager::instance().removeSink(mockSink);
}

void TestCoreLogging::testTaskCancellation()
{
    auto mockSink = std::make_shared<MockCoreLogSink>();
    LogManager::instance().addSink(mockSink);

    auto task = LogManager::instance().createTask(QStringLiteral("Cancellable Task"));
    task->start();
    task->info(QStringLiteral("Processing..."));
    LogManager::instance().flushTask(task->taskId());

    QVERIFY(LogManager::instance().cancelTask(task->taskId(), QStringLiteral("User pressed cancel")));
    QCOMPARE(task->state(), TaskState::Cancelled);
    QCOMPARE(mockSink->taskState(task->taskId()), TaskState::Cancelled);

    LogManager::instance().removeSink(mockSink);
}

void TestCoreLogging::testTaskSkipped()
{
    auto mockSink = std::make_shared<MockCoreLogSink>();
    LogManager::instance().addSink(mockSink);

    auto task = LogManager::instance().createTask(QStringLiteral("Skippable Task"));
    task->start();
    task->info(QStringLiteral("Checking dependencies..."));

    QVERIFY(LogManager::instance().skipTask(task->taskId(), QStringLiteral("Already compiled")));
    QCOMPARE(task->state(), TaskState::Skipped);
    QCOMPARE(mockSink->taskState(task->taskId()), TaskState::Skipped);

    LogManager::instance().removeSink(mockSink);
}

void TestCoreLogging::testConcurrentTasksIsolation()
{
    auto mockSink = std::make_shared<MockCoreLogSink>();
    LogManager::instance().addSink(mockSink);

    const int threadCount = 4;
    const int logsPerThread = 30;
    std::vector<std::thread> threads;
    std::vector<quint64> taskIds(threadCount, 0);

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([i, logsPerThread, &taskIds]() {
            auto task = LogManager::instance().createTask(QStringLiteral("Concurrent Task %1").arg(i));
            taskIds[i] = task->taskId();
            task->start();
            for (int j = 0; j < logsPerThread; ++j) {
                task->info(QStringLiteral("Thread %1 emitting log #%2").arg(i).arg(j));
            }
            LogManager::instance().flushTask(task->taskId());
            LogManager::instance().finishTask(task->taskId(), QStringLiteral("Thread %1 finished").arg(i));
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    QCOMPARE(mockSink->taskCount(), threadCount);
    for (int i = 0; i < threadCount; ++i) {
        quint64 tid = taskIds[i];
        QVERIFY(tid > 0);
        QCOMPARE(mockSink->taskState(tid), TaskState::Completed);
        QStringList msgs = mockSink->taskMessages(tid);
        QVERIFY(msgs.size() >= logsPerThread);
    }

    LogManager::instance().removeSink(mockSink);
}

void TestCoreLogging::testMultiBlockTask()
{
    auto mockSink = std::make_shared<MockCoreLogSink>();
    LogManager::instance().addSink(mockSink);

    auto task = LogManager::instance().createTask(QStringLiteral("Multi Block Task"));
    task->start();

    const int messageCount = 120;
    for (int i = 0; i < messageCount; ++i) {
        task->debug(QStringLiteral("Entry %1").arg(i));
    }

    // Before flushing to sinks, task retains active/uncommitted blocks
    QVector<LogBlock> blocks = LogManager::instance().getAllBlocks(task->taskId());
    QVERIFY(!blocks.isEmpty());

    quint64 totalEntries = 0;
    for (const auto& block : blocks) {
        totalEntries += block.entryCount();
    }
    QVERIFY(totalEntries >= messageCount);

    // Flush to sinks and complete
    LogManager::instance().flushTask(task->taskId());
    LogManager::instance().finishTask(task->taskId(), QStringLiteral("Done"));

    // Once flushed and committed, blocks are delivered to sinks
    QStringList msgs = mockSink->taskMessages(task->taskId());
    QVERIFY(msgs.size() >= messageCount);

    LogManager::instance().removeSink(mockSink);
}

void TestCoreLogging::testLoggedErrorTracking()
{
    auto task = LogManager::instance().createTask(QStringLiteral("Error Tracking"));
    task->start();
    QCOMPARE(task->errorCount(), 0);

    task->warning(QStringLiteral("Minor warning"));
    QCOMPARE(task->errorCount(), 0);

    task->error(QStringLiteral("First error"));
    QCOMPARE(task->errorCount(), 1);

    task->error(QStringLiteral("Second error"));
    QCOMPARE(task->errorCount(), 2);
}

void TestCoreLogging::testInvalidParentTaskRejection()
{
    // Calling createChildTask with a non-existent parent ID (e.g. 999999) must fail/return nullptr
    auto childTask = LogManager::instance().createChildTask(999999, QStringLiteral("Orphaned Child"));
    QVERIFY(childTask == nullptr);

    auto toolTask = LogManager::instance().createToolTask(999999, QStringLiteral("some_tool.exe"));
    QVERIFY(toolTask == nullptr);
}

void TestCoreLogging::testWorkflowLogFolderAndToolFiles()
{
    auto fileSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(fileSink);

    // 1. Create Workflow Root Task
    auto wfTask = LogManager::instance().createWorkflowTask(QStringLiteral("Particle Import Pipeline"), QStringLiteral("explosion"));
    QVERIFY(wfTask != nullptr);
    QVERIFY(wfTask->isWorkflow());
    QVERIFY(!wfTask->workflowDirectory().isEmpty());
    QVERIFY(wfTask->logFilePath().endsWith(QStringLiteral("workflow.log")));
    QCOMPARE(wfTask->assetBaseName(), QStringLiteral("explosion"));

    wfTask->start();
    wfTask->info(QStringLiteral("Workflow started"));
    LogManager::instance().flushTask(wfTask->taskId());

    // Main workflow log file should exist on disk
    QVERIFY(QFileInfo::exists(wfTask->logFilePath()));

    // 2. Create child task inside workflow
    auto childTask = LogManager::instance().createChildTask(
        wfTask->taskId(), QStringLiteral("Extract Stage"));
    QVERIFY(childTask != nullptr);
    QCOMPARE(childTask->workflowDirectory(), wfTask->workflowDirectory());
    QCOMPARE(childTask->assetBaseName(), QStringLiteral("explosion"));

    // 3. Create tool task inside workflow
    auto toolTask = LogManager::instance().createToolTask(
        childTask->taskId(), QStringLiteral("\"C:\\CS2\\tools\\source1import.exe\" -retail"));
    QVERIFY(toolTask != nullptr);
    QVERIFY(toolTask->isToolTask());
    QCOMPARE(toolTask->workflowDirectory(), wfTask->workflowDirectory());
    QCOMPARE(toolTask->assetBaseName(), QStringLiteral("explosion"));

    // Tool log path should be in workflow directory and named explosion_source1import_<time>.log
    QVERIFY(toolTask->logFilePath().startsWith(wfTask->workflowDirectory()));
    QVERIFY(toolTask->logFilePath().contains(QStringLiteral("explosion_source1import_")));
    QVERIFY(toolTask->logFilePath().endsWith(QStringLiteral(".log")));

    toolTask->start();
    toolTask->info(QStringLiteral("Converting PCF asset"));
    LogManager::instance().flushTask(toolTask->taskId());

    // Tool log file should exist on disk
    QVERIFY(QFileInfo::exists(toolTask->logFilePath()));

    LogManager::instance().finishTask(toolTask->taskId(), QStringLiteral("Tool done"));
    LogManager::instance().finishTask(childTask->taskId(), QStringLiteral("Stage done"));
    LogManager::instance().finishTask(wfTask->taskId(), QStringLiteral("Workflow done"));

    LogManager::instance().removeSink(fileSink);
}

void TestCoreLogging::testToolTaskCreationAndStreaming()
{
    auto rootTask = LogManager::instance().createTask(QStringLiteral("Main Import"));
    rootTask->start();
    rootTask->info(QStringLiteral("Root started"));

    auto stageTask = LogManager::instance().createChildTask(rootTask->taskId(), QStringLiteral("Compile Stage"));
    stageTask->start();
    stageTask->info(QStringLiteral("Beginning compilation"));

    // Create tool task
    QString toolCmd = QStringLiteral("\"C:\\CS2\\tools\\resourcecompiler.exe\" -f mesh.vmdl");
    auto toolTask = LogManager::instance().createToolTask(stageTask->taskId(), toolCmd);
    QVERIFY(toolTask != nullptr);
    QVERIFY(toolTask->isToolTask());

    // Parent stageTask should have received [EXEC] entry
    LogManager::instance().flushTask(stageTask->taskId());
    QVector<LogBlock> stageBlocks = LogManager::instance().getAllBlocks(stageTask->taskId());
    bool foundExec = false;
    for (const auto& b : stageBlocks) {
        for (const auto& e : b.entries()) {
            if (e.message.contains(QStringLiteral("[EXEC]")) && e.message.contains(toolCmd)) {
                foundExec = true;
                QCOMPARE(e.toolTaskId, toolTask->taskId());
            }
        }
    }
    QVERIFY(foundExec);

    // Tool task emits output
    toolTask->start();
    toolTask->info(QStringLiteral("Compiling LOD 0"));
    toolTask->info(QStringLiteral("Compiling LOD 1"));
    LogManager::instance().flushTask(toolTask->taskId());

    QVector<LogBlock> toolBlocks = LogManager::instance().getAllBlocks(toolTask->taskId());
    bool foundLOD0 = false;
    for (const auto& b : toolBlocks) {
        for (const auto& e : b.entries()) {
            if (e.message.contains(QStringLiteral("Compiling LOD 0"))) {
                foundLOD0 = true;
            }
        }
    }
    QVERIFY(foundLOD0);

    LogManager::instance().finishTask(toolTask->taskId(), QStringLiteral("Compiled"));
    LogManager::instance().finishTask(stageTask->taskId(), QStringLiteral("Stage finished"));
    LogManager::instance().finishTask(rootTask->taskId(), QStringLiteral("Pipeline finished"));
}

void TestCoreLogging::testProcessRunnerStreamingAndCancellation()
{
    // 1. Line-by-line streaming test
    ProcessOptions opts;
    QStringList linesReceived;
    opts.onStdOutLine = [&linesReceived](const QString& line) {
        linesReceived.append(line.trimmed());
    };

    QStringList cmdArgs;
    cmdArgs << QStringLiteral("/c") << QStringLiteral("echo Alpha && echo Beta && echo Gamma");
    ProcessResult res = ProcessRunner::run(QStringLiteral("cmd.exe"), cmdArgs, opts);

    QVERIFY(res.isSuccess());
    QVERIFY(linesReceived.contains(QStringLiteral("Alpha")));
    QVERIFY(linesReceived.contains(QStringLiteral("Beta")));
    QVERIFY(linesReceived.contains(QStringLiteral("Gamma")));

    // 2. Cooperative cancellation test
    ProcessOptions cancelOpts;
    Core::Async::CancellationToken token;
    cancelOpts.cancellationToken = token;
    cancelOpts.timeout = 10000;

    QStringList pingArgs;
    pingArgs << QStringLiteral("127.0.0.1") << QStringLiteral("-n") << QStringLiteral("10");

    std::thread cancelThread([token]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        token.cancel();
    });

    QElapsedTimer timer;
    timer.start();
    ProcessResult cancelRes = ProcessRunner::run(QStringLiteral("ping.exe"), pingArgs, cancelOpts);
    cancelThread.join();

    QVERIFY(cancelRes.isCancelled());
    // Should be terminated quickly (< 4000ms), far before the 10-second ping finishes
    QVERIFY(timer.elapsed() < 4000);
}

void TestCoreLogging::testProcessResultToErrorMapping()
{
    ProcessResult rSuccess;
    rSuccess.status = ProcessStatus::Success;
    QCOMPARE(rSuccess.toErrorCode(), ErrorCode::Success);
    QVERIFY(rSuccess.toError().isSuccess());

    ProcessResult rTimeout;
    rTimeout.status = ProcessStatus::TimedOut;
    rTimeout.errorMessage = QStringLiteral("Timed out after 30s");
    QCOMPARE(rTimeout.toErrorCode(), ErrorCode::ProcessTimeout);
    QCOMPARE(rTimeout.toError().code(), ErrorCode::ProcessTimeout);
    QCOMPARE(rTimeout.toError().message(), QStringLiteral("Timed out after 30s"));

    ProcessResult rCrash;
    rCrash.status = ProcessStatus::Crashed;
    rCrash.exitCode = -1073741819; // 0xC0000005
    QCOMPARE(rCrash.toErrorCode(), ErrorCode::ProcessCrashed);
    QCOMPARE(rCrash.toError().code(), ErrorCode::ProcessCrashed);

    ProcessResult rFailedStart;
    rFailedStart.status = ProcessStatus::FailedToStart;
    QCOMPARE(rFailedStart.toErrorCode(), ErrorCode::ProcessFailed);

    ProcessResult rExitCode;
    rExitCode.status = ProcessStatus::NonZeroExit;
    rExitCode.exitCode = 1;
    rExitCode.stdErr = QStringLiteral("VMF parsing error at line 42");
    auto exitErr = rExitCode.toError();
    QCOMPARE(exitErr.code(), ErrorCode::ProcessFailed);
    QCOMPARE(exitErr.details(), QStringLiteral("VMF parsing error at line 42"));
}

void TestCoreLogging::testStructuredResultPayload()
{
    // 1. Result<T> with structured Error
    Error err(ErrorCode::FileNotFound, QStringLiteral("materials/models/player/custom.vmt"), QStringLiteral("File lease missing"));
    auto res = Result<int>::failure(err, 12);
    QVERIFY(res.isFailure());
    QCOMPARE(res.errorCode(), ErrorCode::FileNotFound);
    QCOMPARE(res.error().code(), ErrorCode::FileNotFound);
    QCOMPARE(res.message(), QStringLiteral("materials/models/player/custom.vmt"));
    QCOMPARE(res.details(), QStringLiteral("File lease missing"));
    QVERIFY(res.hasValue());
    QCOMPARE(res.value(), 12);

    // 2. Result<void> with ErrorCode and string
    auto voidRes = Result<void>::failure(ErrorCode::DirectoryNotFound, QStringLiteral("csgo/maps directory missing"));
    QVERIFY(voidRes.isFailure());
    QCOMPARE(voidRes.errorCode(), ErrorCode::DirectoryNotFound);
    QCOMPARE(voidRes.message(), QStringLiteral("csgo/maps directory missing"));
}

void TestCoreLogging::testResultOutcomes()
{
    // 1. Success
    auto s = Result<QString>::success(QStringLiteral("ok"), QStringLiteral("Operation succeeded"));
    QVERIFY(s.isSuccess());
    QCOMPARE(s.value(), QStringLiteral("ok"));
    QCOMPARE(s.message(), QStringLiteral("Operation succeeded"));

    // 2. Failure
    auto f = Result<int>::failure(ErrorCode::WriteFailed, QStringLiteral("No disk space"));
    QVERIFY(f.isFailure());
    QCOMPARE(f.errorCode(), ErrorCode::WriteFailed);
    QCOMPARE(f.message(), QStringLiteral("No disk space"));

    // 3. Cancelled
    auto c = Result<int>::cancelled(QStringLiteral("User cancelled"));
    QVERIFY(c.isCancelled());
    QCOMPARE(c.message(), QStringLiteral("User cancelled"));

    // 4. Skipped
    auto sk = Result<void>::skipped(QStringLiteral("Already imported"));
    QVERIFY(sk.isSkipped());
    QCOMPARE(sk.message(), QStringLiteral("Already imported"));
}

QTEST_GUILESS_MAIN(TestCoreLogging)
#include "TestCoreLogging.moc"
