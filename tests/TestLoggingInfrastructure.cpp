#include <QtTest/QtTest>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <memory>

#include "Core/Logging/ApplicationLogger.h"
#include "Core/Logging/LogFileManager.h"
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
    void testTaskCompletionFlushesToFile();
    void testTaskFailureRecordsError();
    void testTaskCancellationRecordsWarning();
    void testExternalToolLogIsolation();
    void testApplicationLogIsolation();
    void testOpenLogFileSelectionHierarchy();

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

QTEST_MAIN(TestLoggingInfrastructure)
#include "TestLoggingInfrastructure.moc"
