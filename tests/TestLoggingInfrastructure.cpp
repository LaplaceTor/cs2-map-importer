#include <QtTest/QtTest>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <memory>

#include "Core/Logging/ApplicationLogger.h"
#include "Core/Logging/LogFileManager.h"
#include "Core/Logging/Logger.h"
#include "Core/Logging/LogManager.h"
#include "Core/Logging/LogSource.h"
#include "Core/Logging/TaskFileSink.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "UI/ViewModels/LogViewModel.h"

using namespace Core::Logging;
using namespace UI::ViewModels;

class TestLoggingInfrastructure : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testLogFileManagerSanitization();
    void testApplicationLogCreationAndNaming();
    void testTaskLogCreationAndNaming();
    void testTaskFileCreatedImmediatelyAtTaskCreation();
    void testTaskCompletionFlushesToFile();
    void testTaskFileClosedOnCompletion();
    void testLogManagerClearClosesTaskFiles();
    void testDynamicAddSinkSkipsCompletedTasks();
    void testTaskFailureRecordsError();
    void testTaskCancellationRecordsWarning();
    void testExternalToolLogIsolation();
    void testApplicationLogIsolation();
    void testLegacyLoggerRedirectsToApplicationLog();
    void testOpenLogFileSelectionHierarchy();
    void testDuplicateTaskNameLogFileUniqueness();
    void testLogFileReadySemantics();
    void testResetViewDoesNotTerminateTask();
    void testResetViewKeepsDiskLog();
    void testResetViewOnlyChangesUi();
    void testWorkflowFailureKeepsUiLogs();
    void testNextWorkflowResetsUiKeepsDiskLogs();
    void testOpenLogFilePointsToLatestTaskAfterReset();
    void testResetViewDoesNotResetSinkCursor();

private:
    std::unique_ptr<QTemporaryDir> m_tempDir;
    QString m_originalLogsDir;
};

void TestLoggingInfrastructure::initTestCase()
{
    m_tempDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_tempDir->isValid());
    m_originalLogsDir = LogFileManager::logsDirectory();
    LogFileManager::setLogsDirectory(m_tempDir->path());
}

void TestLoggingInfrastructure::cleanupTestCase()
{
    ApplicationLogger::shutdown();
    LogFileManager::setLogsDirectory(m_originalLogsDir);
    m_tempDir.reset();
}

void TestLoggingInfrastructure::init()
{
    LogManager::instance().clear();
    ApplicationLogger::shutdown();
}

void TestLoggingInfrastructure::cleanup()
{
    ApplicationLogger::shutdown();
    LogManager::instance().clear();
}

void TestLoggingInfrastructure::testLogFileManagerSanitization()
{
    // Test illegal character replacement
    QString dirtyName = QStringLiteral("Map: Import / CS2 * Test? <1> | \"Foo\"");
    QString cleanName = LogFileManager::sanitizeFileName(dirtyName);
    QVERIFY(!cleanName.contains(QLatin1Char(':')));
    QVERIFY(!cleanName.contains(QLatin1Char('/')));
    QVERIFY(!cleanName.contains(QLatin1Char('*')));
    QVERIFY(!cleanName.contains(QLatin1Char('?')));
    QVERIFY(!cleanName.contains(QLatin1Char('<')));
    QVERIFY(!cleanName.contains(QLatin1Char('>')));
    QVERIFY(!cleanName.contains(QLatin1Char('|')));
    QVERIFY(!cleanName.contains(QLatin1Char('"')));

    // Test fallback for empty or completely illegal names
    QCOMPARE(LogFileManager::sanitizeFileName(QStringLiteral("   ")), QStringLiteral("task"));
    QCOMPARE(LogFileManager::sanitizeFileName(QStringLiteral("///:::***")), QStringLiteral("task"));

    // Test length truncation
    QString superLongName(100, QLatin1Char('A'));
    QString truncated = LogFileManager::sanitizeFileName(superLongName);
    QVERIFY(truncated.length() <= 64);
}

void TestLoggingInfrastructure::testApplicationLogCreationAndNaming()
{
    const qint64 time1 = 1700000000000;
    const QString appLogPath1 = LogFileManager::generateApplicationLogFilePath(time1);
    QVERIFY(appLogPath1.endsWith(QStringLiteral("application_20231114_221320_000.log")));

    bool initOk = ApplicationLogger::initialize(appLogPath1);
    QVERIFY(initOk);
    QVERIFY(ApplicationLogger::isInitialized());
    QCOMPARE(ApplicationLogger::logFilePath(), appLogPath1);

    ApplicationLogger::info(QStringLiteral("App startup test event"));
    ApplicationLogger::shutdown();

    QVERIFY(QFile::exists(appLogPath1));
    QFile file(appLogPath1);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("[Application]")));
    QVERIFY(content.contains(QStringLiteral("[INFO] App startup test event")));
}

void TestLoggingInfrastructure::testTaskLogCreationAndNaming()
{
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    auto task = LogManager::instance().createTask(QStringLiteral("Map Import De_Dust2"));
    QVERIFY(task != nullptr);
    task->start();
    task->info(QStringLiteral("Extracting pak01_dir.vpk"));

    LogManager::instance().flushTask(task->taskId());

    const QString taskPath = taskSink->taskLogFilePath(task->taskId());
    QVERIFY(!taskPath.isEmpty());
    QVERIFY(taskPath.contains(QStringLiteral("Map_Import_De_Dust2")));
    QVERIFY(QFile::exists(taskPath));

    QFile file(taskPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("Map Import De_Dust2")));
    QVERIFY(content.contains(QStringLiteral("[Source: Workflow]")));
    QVERIFY(content.contains(QStringLiteral("Extracting pak01_dir.vpk")));

    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testTaskFileCreatedImmediatelyAtTaskCreation()
{
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    // Create task but DO NOT log anything or call start()
    auto task = LogManager::instance().createTask(QStringLiteral("Instant File Creation Task"));
    QVERIFY(task != nullptr);

    // Task log file MUST exist on disk immediately upon createTask()
    const QString taskPath = taskSink->taskLogFilePath(task->taskId());
    QVERIFY(!taskPath.isEmpty());
    QCOMPARE(taskPath, task->logFilePath());
    QVERIFY(QFile::exists(taskPath));

    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testTaskCompletionFlushesToFile()
{
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    auto task = LogManager::instance().createTask(QStringLiteral("Compile VMAT"));
    task->start();
    task->info(QStringLiteral("Compiling materials..."));
    task->complete(QStringLiteral("All materials compiled successfully"));

    LogManager::instance().flushTask(task->taskId());

    const QString taskPath = taskSink->taskLogFilePath(task->taskId());
    QVERIFY(QFile::exists(taskPath));

    QFile file(taskPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("All materials compiled successfully")));

    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testTaskFileClosedOnCompletion()
{
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    auto task = LogManager::instance().createTask(QStringLiteral("Closure Test Task"));
    task->start();
    task->info(QStringLiteral("Task running"));

    const QString taskPath = taskSink->taskLogFilePath(task->taskId());
    QVERIFY(QFile::exists(taskPath));
    QVERIFY(taskSink->isTaskFileOpen(task->taskId()));

    // Finish task through LogManager
    LogManager::instance().finishTask(task->taskId(), QStringLiteral("Task completed"));

    // Verify handle is closed
    QVERIFY(!taskSink->isTaskFileOpen(task->taskId()));

    // Verify file can be read
    QFile file(taskPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("Task completed")));
    file.close();

    // Verify on Windows that file handle is closed by successfully renaming and deleting it
    const QString renamedPath = taskPath + QStringLiteral(".renamed");
    QVERIFY(QFile::rename(taskPath, renamedPath));
    QVERIFY(QFile::exists(renamedPath));
    QVERIFY(QFile::remove(renamedPath));
    QVERIFY(!QFile::exists(renamedPath));

    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testLogManagerClearClosesTaskFiles()
{
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    auto task1 = LogManager::instance().createTask(QStringLiteral("Clear Test Task 1"));
    auto task2 = LogManager::instance().createTask(QStringLiteral("Clear Test Task 2"));
    task1->info(QStringLiteral("Running 1"));
    task2->info(QStringLiteral("Running 2"));

    const QString path1 = taskSink->taskLogFilePath(task1->taskId());
    const QString path2 = taskSink->taskLogFilePath(task2->taskId());
    QVERIFY(QFile::exists(path1));
    QVERIFY(QFile::exists(path2));
    QVERIFY(taskSink->isTaskFileOpen(task1->taskId()));
    QVERIFY(taskSink->isTaskFileOpen(task2->taskId()));

    // Calling clear() must flush and close all files cleanly
    LogManager::instance().clear();

    // Verify file handles are closed by successfully renaming and removing them
    const QString renamed1 = path1 + QStringLiteral(".renamed");
    QVERIFY(QFile::rename(path1, renamed1));
    QVERIFY(QFile::remove(renamed1));

    const QString renamed2 = path2 + QStringLiteral(".renamed");
    QVERIFY(QFile::rename(path2, renamed2));
    QVERIFY(QFile::remove(renamed2));
}

void TestLoggingInfrastructure::testDynamicAddSinkSkipsCompletedTasks()
{
    auto initialSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(initialSink);

    auto task = LogManager::instance().createTask(QStringLiteral("Dynamic Sink Test Task"));
    task->start();
    task->info(QStringLiteral("Task finished"));
    LogManager::instance().finishTask(task->taskId(), QStringLiteral("Done"));

    // Task is now Completed. Add a new TaskFileSink dynamically
    auto lateSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(lateSink);

    // lateSink should NOT open a file handle for completed task
    QVERIFY(!lateSink->isTaskFileOpen(task->taskId()));

    LogManager::instance().removeSink(initialSink);
    LogManager::instance().removeSink(lateSink);
}

void TestLoggingInfrastructure::testTaskFailureRecordsError()
{
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    auto task = LogManager::instance().createTask(QStringLiteral("Compile Models"));
    task->start();
    task->info(QStringLiteral("Compiling MDL..."));
    task->fail(QStringLiteral("MDL compiler returned error 127"));

    LogManager::instance().flushTask(task->taskId());

    const QString taskPath = taskSink->taskLogFilePath(task->taskId());
    QVERIFY(QFile::exists(taskPath));

    QFile file(taskPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("[ERROR] MDL compiler returned error 127")));

    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testTaskCancellationRecordsWarning()
{
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    auto task = LogManager::instance().createTask(QStringLiteral("Long Import Pipeline"));
    task->start();
    task->info(QStringLiteral("Starting heavy step..."));
    task->cancel(QStringLiteral("User cancelled import"));

    LogManager::instance().flushTask(task->taskId());

    const QString taskPath = taskSink->taskLogFilePath(task->taskId());
    QVERIFY(QFile::exists(taskPath));

    QFile file(taskPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("[WARNING] User cancelled import")));

    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testExternalToolLogIsolation()
{
    // Initialize ApplicationLogger
    const QString appLogPath = LogFileManager::generateApplicationLogFilePath(1700000000001);
    ApplicationLogger::initialize(appLogPath);

    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    auto task = LogManager::instance().createTask(QStringLiteral("External Tool Step"));
    task->start();
    task->logExternalToolOutput(QStringLiteral("VMDL_COMPILER: Processing mesh 0"));
    task->logExternalToolOutput(QStringLiteral("VMDL_COMPILER ERROR: missing bone weights"), LogLevel::Error);

    LogManager::instance().flushTask(task->taskId());
    ApplicationLogger::shutdown();

    // Check Task Log: MUST contain the external tool output
    const QString taskPath = taskSink->taskLogFilePath(task->taskId());
    QVERIFY(QFile::exists(taskPath));
    QFile taskFile(taskPath);
    QVERIFY(taskFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString taskContent = QString::fromUtf8(taskFile.readAll());
    QVERIFY(taskContent.contains(QStringLiteral("[Source: ExternalTool]")));
    QVERIFY(taskContent.contains(QStringLiteral("VMDL_COMPILER: Processing mesh 0")));
    QVERIFY(taskContent.contains(QStringLiteral("VMDL_COMPILER ERROR: missing bone weights")));

    // Check Application Log: MUST NOT contain external tool stdout/stderr
    QVERIFY(QFile::exists(appLogPath));
    QFile appFile(appLogPath);
    QVERIFY(appFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString appContent = QString::fromUtf8(appFile.readAll());
    QVERIFY(!appContent.contains(QStringLiteral("VMDL_COMPILER")));

    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testApplicationLogIsolation()
{
    const QString appLogPath = LogFileManager::generateApplicationLogFilePath(1700000000002);
    ApplicationLogger::initialize(appLogPath);

    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    auto task = LogManager::instance().createTask(QStringLiteral("Isolated Task"));
    task->start();
    task->info(QStringLiteral("Task-specific internal message"));

    ApplicationLogger::info(QStringLiteral("Global service initialized"));

    LogManager::instance().flushTask(task->taskId());
    ApplicationLogger::shutdown();

    // Task log should contain task-specific log but NOT application logger message
    const QString taskPath = taskSink->taskLogFilePath(task->taskId());
    QFile taskFile(taskPath);
    QVERIFY(taskFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString taskContent = QString::fromUtf8(taskFile.readAll());
    QVERIFY(taskContent.contains(QStringLiteral("Task-specific internal message")));
    QVERIFY(!taskContent.contains(QStringLiteral("Global service initialized")));

    // Application log should contain app message but NOT task message
    QFile appFile(appLogPath);
    QVERIFY(appFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString appContent = QString::fromUtf8(appFile.readAll());
    QVERIFY(appContent.contains(QStringLiteral("Global service initialized")));
    QVERIFY(!appContent.contains(QStringLiteral("Task-specific internal message")));

    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testLegacyLoggerRedirectsToApplicationLog()
{
    const qint64 appTime = 1700000002000;
    const QString appLogPath = LogFileManager::generateApplicationLogFilePath(appTime);
    ApplicationLogger::initialize(appTime, appLogPath);

    // Call legacy Logger APIs
    Core::Logging::Logger::info(QStringLiteral("Legacy logger info message"));
    Core::Logging::Logger::warning(QStringLiteral("Legacy logger warning message"));
    Core::Logging::Logger::error(QStringLiteral("Legacy logger error message"));

    ApplicationLogger::shutdown();

    QVERIFY(QFile::exists(appLogPath));
    QFile file(appLogPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("[INFO] Legacy logger info message")));
    QVERIFY(content.contains(QStringLiteral("[WARNING] Legacy logger warning message")));
    QVERIFY(content.contains(QStringLiteral("[ERROR] Legacy logger error message")));
}

void TestLoggingInfrastructure::testOpenLogFileSelectionHierarchy()
{
    const QString appLogPath = LogFileManager::generateApplicationLogFilePath(1700000000003);
    ApplicationLogger::initialize(appLogPath);
    ApplicationLogger::info(QStringLiteral("App startup"));

    LogViewModel logVm;
    logVm.registerWithLogManager();

    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    // 1. When no task has run: active and last task paths are empty
    QCOMPARE(logVm.activeTaskLogFilePath(), QString());
    QCOMPARE(logVm.lastTaskLogFilePath(), QString());

    // 2. Start Task 1 -> activeTaskLogFilePath and lastTaskLogFilePath point to Task 1
    auto task1 = LogManager::instance().createTask(QStringLiteral("Task One"));
    task1->start();
    task1->info(QStringLiteral("Working 1"));
    LogManager::instance().flushTask(task1->taskId());

    QTRY_VERIFY(!logVm.activeTaskLogFilePath().isEmpty());
    QCOMPARE(logVm.activeTaskLogFilePath(), logVm.lastTaskLogFilePath());

    // 3. Complete Task 1 -> active becomes empty, last remains Task 1
    LogManager::instance().finishTask(task1->taskId(), "Finished 1");
    LogManager::instance().flushTask(task1->taskId());

    QTRY_COMPARE(logVm.activeTaskLogFilePath(), QString());
    QVERIFY(!logVm.lastTaskLogFilePath().isEmpty());
    QVERIFY(logVm.lastTaskLogFilePath().contains(QStringLiteral("Task_One")));

    // 4. Start Task 2 -> active becomes Task 2, last becomes Task 2
    auto task2 = LogManager::instance().createTask(QStringLiteral("Task Two"));
    task2->start();
    task2->info(QStringLiteral("Working 2"));
    LogManager::instance().flushTask(task2->taskId());

    QTRY_VERIFY(logVm.activeTaskLogFilePath().contains(QStringLiteral("Task_Two")));
    QCOMPARE(logVm.activeTaskLogFilePath(), logVm.lastTaskLogFilePath());

    LogManager::instance().finishTask(task2->taskId(), "Finished 2");
    LogManager::instance().flushTask(task2->taskId());

    logVm.unregisterFromLogManager();
    LogManager::instance().removeSink(taskSink);
    ApplicationLogger::shutdown();
}

void TestLoggingInfrastructure::testDuplicateTaskNameLogFileUniqueness()
{
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    // Create two distinct tasks with the exact same name
    auto task1 = LogManager::instance().createTask(QStringLiteral("Map Import"));
    auto task2 = LogManager::instance().createTask(QStringLiteral("Map Import"));

    QVERIFY(task1 != nullptr);
    QVERIFY(task2 != nullptr);
    QVERIFY(task1->taskId() != task2->taskId());

    const QString path1 = task1->logFilePath();
    const QString path2 = task2->logFilePath();

    // Critical invariant: Even with the identical task name and identical second timestamp,
    // unique task IDs ensure distinct log file paths.
    QVERIFY(!path1.isEmpty());
    QVERIFY(!path2.isEmpty());
    QVERIFY(path1 != path2);

    QVERIFY(QFile::exists(path1));
    QVERIFY(QFile::exists(path2));

    task1->start();
    task1->info(QStringLiteral("Task 1 specific processing"));
    task2->start();
    task2->info(QStringLiteral("Task 2 specific processing"));

    LogManager::instance().finishTask(task1->taskId(), QStringLiteral("Task 1 finished"));
    LogManager::instance().finishTask(task2->taskId(), QStringLiteral("Task 2 finished"));

    // Verify isolation in log files
    QFile file1(path1);
    QVERIFY(file1.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content1 = QString::fromUtf8(file1.readAll());
    file1.close();

    QFile file2(path2);
    QVERIFY(file2.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content2 = QString::fromUtf8(file2.readAll());
    file2.close();

    QVERIFY(content1.contains(QStringLiteral("Task 1 specific processing")));
    QVERIFY(content1.contains(QStringLiteral("Task 1 finished")));
    QVERIFY(!content1.contains(QStringLiteral("Task 2 specific processing")));

    QVERIFY(content2.contains(QStringLiteral("Task 2 specific processing")));
    QVERIFY(content2.contains(QStringLiteral("Task 2 finished")));
    QVERIFY(!content2.contains(QStringLiteral("Task 1 specific processing")));

    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testLogFileReadySemantics()
{
    // 1. When no sink is registered, createTask produces a context where isLogFileReady() == false
    auto taskNoSink = LogManager::instance().createTask(QStringLiteral("No Sink Task"));
    QVERIFY(taskNoSink != nullptr);
    QVERIFY(!taskNoSink->isLogFileReady());

    // 2. Add TaskFileSink dynamically -> taskNoSink is notified and isLogFileReady() becomes true
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);
    QVERIFY(taskNoSink->isLogFileReady());
    QVERIFY(taskSink->hasTaskLogFile(taskNoSink->taskId()));
    QVERIFY(taskSink->isTaskFileOpen(taskNoSink->taskId()));
    QVERIFY(QFile::exists(taskNoSink->logFilePath()));

    // 3. Creating a new task with TaskFileSink registered results in isLogFileReady() == true
    auto taskWithSink = LogManager::instance().createTask(QStringLiteral("With Sink Task"));
    QVERIFY(taskWithSink != nullptr);
    QVERIFY(taskWithSink->isLogFileReady());
    QVERIFY(taskSink->hasTaskLogFile(taskWithSink->taskId()));
    QVERIFY(taskSink->isTaskFileOpen(taskWithSink->taskId()));
    QVERIFY(QFile::exists(taskWithSink->logFilePath()));

    // 4. Complete taskWithSink: file handle closes, but log file remains ready and available on disk
    taskWithSink->start();
    taskWithSink->info(QStringLiteral("Some execution details"));
    LogManager::instance().finishTask(taskWithSink->taskId(), QStringLiteral("Done"));
    QVERIFY(!taskSink->isTaskFileOpen(taskWithSink->taskId())); // Write handle closed
    QVERIFY(taskSink->hasTaskLogFile(taskWithSink->taskId()));  // Log file physically exists
    QVERIFY(taskWithSink->isLogFileReady());                     // Context still reports file ready

    // 5. Remove TaskFileSink -> isLogFileReady() becomes false for active/tracked tasks
    LogManager::instance().removeSink(taskSink);
    QVERIFY(!taskNoSink->isLogFileReady());
    QVERIFY(!taskWithSink->isLogFileReady());
}

void TestLoggingInfrastructure::testResetViewDoesNotTerminateTask()
{
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    LogViewModel logVm;
    logVm.registerWithLogManager();

    auto task = LogManager::instance().createTask(QStringLiteral("Non Terminating Task"));
    QVERIFY(task != nullptr);
    task->start();
    task->info(QStringLiteral("Log A before reset"));
    LogManager::instance().flushTask(task->taskId());

    QTRY_COMPARE(logVm.totalMessageCount(), 1);

    // Reset UI view
    logVm.resetView();

    // Verify task state remains running, context remains active, file sink handle remains open
    QCOMPARE(task->state(), TaskState::Running);
    QVERIFY(!TaskLoggingContext::isTerminalState(task->state()));
    QVERIFY(taskSink->isTaskFileOpen(task->taskId()));

    // Write Log B after reset
    task->info(QStringLiteral("Log B after reset"));
    LogManager::instance().flushTask(task->taskId());

    // Task still running and healthy
    QCOMPARE(task->state(), TaskState::Running);
    QVERIFY(taskSink->isTaskFileOpen(task->taskId()));

    LogManager::instance().finishTask(task->taskId(), QStringLiteral("Task completed"));
    logVm.unregisterFromLogManager();
    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testResetViewKeepsDiskLog()
{
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    LogViewModel logVm;
    logVm.registerWithLogManager();

    auto task = LogManager::instance().createTask(QStringLiteral("Disk Log Persistence Task"));
    task->start();
    task->info(QStringLiteral("Log A entry"));
    LogManager::instance().flushTask(task->taskId());

    // UI View Reset
    logVm.resetView();

    task->info(QStringLiteral("Log B entry"));
    task->complete(QStringLiteral("All done"));
    LogManager::instance().flushTask(task->taskId());

    const QString taskPath = taskSink->taskLogFilePath(task->taskId());
    QVERIFY(QFile::exists(taskPath));

    QFile file(taskPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("Log A entry")));
    QVERIFY(content.contains(QStringLiteral("Log B entry")));
    QVERIFY(content.contains(QStringLiteral("All done")));

    logVm.unregisterFromLogManager();
    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testResetViewOnlyChangesUi()
{
    LogViewModel logVm;
    logVm.registerWithLogManager();

    auto task = LogManager::instance().createTask(QStringLiteral("UI Isolation Task"));
    task->start();
    task->info(QStringLiteral("Message Alpha"));
    LogManager::instance().flushTask(task->taskId());

    QTRY_COMPARE(logVm.totalMessageCount(), 1);
    QCOMPARE(logVm.taskCount(), 1);
    QVERIFY(logVm.getFullLogText().contains(QStringLiteral("Message Alpha")));

    // Perform resetView
    logVm.resetView();

    QCOMPARE(logVm.totalMessageCount(), 0);
    QCOMPARE(logVm.taskCount(), 0);
    QVERIFY(logVm.getFullLogText().isEmpty());

    // New log message arrives
    task->info(QStringLiteral("Message Beta"));
    LogManager::instance().flushTask(task->taskId());

    QTRY_COMPARE(logVm.totalMessageCount(), 1);
    QCOMPARE(logVm.taskCount(), 1);
    QString textAfter = logVm.getFullLogText();
    QVERIFY(textAfter.contains(QStringLiteral("Message Beta")));
    QVERIFY(!textAfter.contains(QStringLiteral("Message Alpha")));

    LogManager::instance().finishTask(task->taskId(), QStringLiteral("Finished"));
    logVm.unregisterFromLogManager();
}

void TestLoggingInfrastructure::testWorkflowFailureKeepsUiLogs()
{
    LogViewModel logVm;
    logVm.registerWithLogManager();

    auto task = LogManager::instance().createTask(QStringLiteral("Failing Workflow Task"));
    task->start();
    task->info(QStringLiteral("Starting step that fails"));
    task->fail(QStringLiteral("Critical error encountered in asset conversion"));
    LogManager::instance().flushTask(task->taskId());

    QTRY_COMPARE(logVm.totalMessageCount(), 2);
    QCOMPARE(logVm.taskCount(), 1);

    // Verify UI still retains the full failure logs and state
    QModelIndex idx = logVm.index(0, 0);
    QCOMPARE(logVm.data(idx, LogTaskModel::StateStringRole).toString(), QStringLiteral("FAILED"));
    QString fullText = logVm.getFullLogText();
    QVERIFY(fullText.contains(QStringLiteral("Starting step that fails")));
    QVERIFY(fullText.contains(QStringLiteral("Critical error encountered in asset conversion")));

    logVm.unregisterFromLogManager();
}

void TestLoggingInfrastructure::testNextWorkflowResetsUiKeepsDiskLogs()
{
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    LogViewModel logVm;
    logVm.registerWithLogManager();

    // 1. Run Workflow A
    auto taskA = LogManager::instance().createTask(QStringLiteral("Workflow A"));
    taskA->start();
    taskA->info(QStringLiteral("Log A message"));
    taskA->complete(QStringLiteral("Workflow A succeeded"));
    LogManager::instance().flushTask(taskA->taskId());

    QTRY_COMPARE(logVm.taskCount(), 1);
    QVERIFY(logVm.getFullLogText().contains(QStringLiteral("Log A message")));

    const QString pathA = taskSink->taskLogFilePath(taskA->taskId());

    // 2. Start Workflow B: resetView is invoked at workflow acceptance point
    logVm.resetView();

    auto taskB = LogManager::instance().createTask(QStringLiteral("Workflow B"));
    taskB->start();
    taskB->info(QStringLiteral("Log B message"));
    taskB->complete(QStringLiteral("Workflow B succeeded"));
    LogManager::instance().flushTask(taskB->taskId());

    QTRY_COMPARE(logVm.taskCount(), 1);
    QString textB = logVm.getFullLogText();
    QVERIFY(textB.contains(QStringLiteral("Log B message")));
    QVERIFY(!textB.contains(QStringLiteral("Log A message")));

    const QString pathB = taskSink->taskLogFilePath(taskB->taskId());

    // 3. Verify disk logs: both file A and file B exist and contain their respective logs
    QVERIFY(QFile::exists(pathA));
    QVERIFY(QFile::exists(pathB));
    QVERIFY(pathA != pathB);

    QFile fileA(pathA);
    QVERIFY(fileA.open(QIODevice::ReadOnly | QIODevice::Text));
    QString contentA = QString::fromUtf8(fileA.readAll());
    QVERIFY(contentA.contains(QStringLiteral("Log A message")));
    QVERIFY(!contentA.contains(QStringLiteral("Log B message")));

    QFile fileB(pathB);
    QVERIFY(fileB.open(QIODevice::ReadOnly | QIODevice::Text));
    QString contentB = QString::fromUtf8(fileB.readAll());
    QVERIFY(contentB.contains(QStringLiteral("Log B message")));
    QVERIFY(!contentB.contains(QStringLiteral("Log A message")));

    logVm.unregisterFromLogManager();
    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testOpenLogFilePointsToLatestTaskAfterReset()
{
    auto taskSink = std::make_shared<TaskFileSink>();
    LogManager::instance().addSink(taskSink);

    LogViewModel logVm;
    logVm.registerWithLogManager();

    // Run Task 1
    auto task1 = LogManager::instance().createTask(QStringLiteral("First Task"));
    task1->start();
    task1->info(QStringLiteral("Task 1 processing"));
    LogManager::instance().finishTask(task1->taskId(), QStringLiteral("Task 1 done"));
    LogManager::instance().flushTask(task1->taskId());

    QTRY_COMPARE(logVm.taskCount(), 1);
    QVERIFY(logVm.lastTaskLogFilePath().contains(QStringLiteral("First_Task")));

    // Reset UI before Workflow 2 starts
    logVm.resetView();
    QCOMPARE(logVm.activeTaskLogFilePath(), QString());
    QCOMPARE(logVm.lastTaskLogFilePath(), QString());

    // Start Task 2
    auto task2 = LogManager::instance().createTask(QStringLiteral("Second Task"));
    task2->start();
    task2->info(QStringLiteral("Task 2 processing"));
    LogManager::instance().flushTask(task2->taskId());

    QTRY_VERIFY(logVm.activeTaskLogFilePath().contains(QStringLiteral("Second_Task")));
    QCOMPARE(logVm.activeTaskLogFilePath(), logVm.lastTaskLogFilePath());

    LogManager::instance().finishTask(task2->taskId(), QStringLiteral("Task 2 done"));
    logVm.unregisterFromLogManager();
    LogManager::instance().removeSink(taskSink);
}

void TestLoggingInfrastructure::testResetViewDoesNotResetSinkCursor()
{
    LogViewModel logVm;
    logVm.registerWithLogManager();

    auto task = LogManager::instance().createTask(QStringLiteral("Cursor Test Task"));
    task->start();

    // Write entries to seal multiple blocks
    task->info(QStringLiteral("Block entry 1"));
    LogManager::instance().flushTask(task->taskId());

    task->info(QStringLiteral("Block entry 2"));
    LogManager::instance().flushTask(task->taskId());

    QTRY_COMPARE(logVm.totalMessageCount(), 2);

    // Reset UI view
    logVm.resetView();
    QCOMPARE(logVm.totalMessageCount(), 0);

    // Write third entry (Block 3)
    task->info(QStringLiteral("Block entry 3"));
    LogManager::instance().flushTask(task->taskId());

    // UI should only receive Block 3, NOT duplicate Block 1 and Block 2
    QTRY_COMPARE(logVm.totalMessageCount(), 1);
    QString fullText = logVm.getFullLogText();
    QVERIFY(fullText.contains(QStringLiteral("Block entry 3")));
    QVERIFY(!fullText.contains(QStringLiteral("Block entry 1")));
    QVERIFY(!fullText.contains(QStringLiteral("Block entry 2")));

    LogManager::instance().finishTask(task->taskId(), QStringLiteral("Completed"));
    logVm.unregisterFromLogManager();
}

QTEST_MAIN(TestLoggingInfrastructure)
#include "TestLoggingInfrastructure.moc"
