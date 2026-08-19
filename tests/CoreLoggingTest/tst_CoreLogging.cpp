#include <QtTest>
#include <QThreadPool>
#include <QRunnable>
#include <QSet>
#include <QList>
#include <algorithm>

#include "Core/Logging/LogBlock.h"
#include "Core/Logging/LogEntry.h"
#include "Core/Logging/LogLevel.h"
#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskContext.h"
#include "Core/Logging/TaskState.h"

using namespace Core::Logging;

class CoreLoggingTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void testLogBlockSealedBehavior();
    void testTaskContextStateAndProgress();
    void testTaskContextBlockRotation();
    void testLogManagerBasic();
    void testMultiThreadedTaskCreation();
    void testMultiThreadedLoggingAndIsolation();
};

void CoreLoggingTest::init()
{
    LogManager::instance().resetForTesting();
}

void CoreLoggingTest::testLogBlockSealedBehavior()
{
    LogBlock block;
    QCOMPARE(block.isSealed(), false);
    QCOMPARE(block.entryCount(), 0);
    QCOMPARE(block.size(), 0);

    LogEntry entry;
    entry.sequence = 1;
    entry.message = "Hello World";

    QVERIFY(block.append(entry));
    QCOMPARE(block.entryCount(), 1);
    QVERIFY(block.size() > 0);

    block.seal();
    QCOMPARE(block.isSealed(), true);

    // Appending to sealed block must fail and return false
    LogEntry entry2;
    entry2.sequence = 2;
    entry2.message = "After seal";
    QCOMPARE(block.append(entry2), false);
    QCOMPARE(block.entryCount(), 1);
}

void CoreLoggingTest::testTaskContextStateAndProgress()
{
    TaskContext task(100, "TestTask");
    QCOMPARE(task.taskId(), quint64(100));
    QCOMPARE(task.taskName(), QString("TestTask"));
    QCOMPARE(task.state(), TaskState::Pending);
    QCOMPARE(task.progress(), 0.0);
    QCOMPARE(task.isFinished(), false);

    task.setState(TaskState::Running);
    QCOMPARE(task.state(), TaskState::Running);

    task.setProgress(0.5);
    QCOMPARE(task.progress(), 0.5);

    // Test clamping
    task.setProgress(1.5);
    QCOMPARE(task.progress(), 1.0);
    task.setProgress(-0.5);
    QCOMPARE(task.progress(), 0.0);

    task.setCurrentMessage("Processing...");
    QCOMPARE(task.currentMessage(), QString("Processing..."));

    task.complete("Done!");
    QCOMPARE(task.state(), TaskState::Completed);
    QCOMPARE(task.progress(), 1.0);
    QCOMPARE(task.currentMessage(), QString("Done!"));
    QCOMPARE(task.isFinished(), true);
}

void CoreLoggingTest::testTaskContextBlockRotation()
{
    TaskContext task(1, "RotationTask");
    // Set low thresholds for testing rotation
    task.setRotationThresholds(2, 1024); // max 2 entries per block

    task.info("Message 1");
    task.info("Message 2");
    QCOMPARE(task.blocks().size(), 1);
    QCOMPARE(task.totalEntryCount(), 2);

    // Third message should trigger rotation to block 2
    task.info("Message 3");
    QCOMPARE(task.blocks().size(), 2);
    QCOMPARE(task.blocks().at(0).isSealed(), true);
    QCOMPARE(task.blocks().at(0).entryCount(), 2);
    QCOMPARE(task.blocks().at(1).isSealed(), false);
    QCOMPARE(task.blocks().at(1).entryCount(), 1);
    QCOMPARE(task.totalEntryCount(), 3);
}

void CoreLoggingTest::testLogManagerBasic()
{
    auto& manager = LogManager::instance();
    auto task1 = manager.createTask("Task1");
    auto task2 = manager.createTask("Task2");

    QVERIFY(task1 != nullptr);
    QVERIFY(task2 != nullptr);
    QVERIFY(task1->taskId() != task2->taskId());
    QCOMPARE(task1->taskName(), QString("Task1"));
    QCOMPARE(task2->taskName(), QString("Task2"));

    QCOMPARE(manager.findTask(task1->taskId()), task1);
    QCOMPARE(manager.findTask(task2->taskId()), task2);

    manager.finishTask(task1->taskId(), "Finished Task 1");
    QCOMPARE(task1->state(), TaskState::Completed);

    manager.failTask(task2->taskId(), "Failed Task 2");
    QCOMPARE(task2->state(), TaskState::Failed);
}

class TaskCreatorRunnable : public QRunnable
{
public:
    TaskCreatorRunnable(int count) : m_count(count) {}
    void run() override
    {
        for (int i = 0; i < m_count; ++i) {
            auto task = LogManager::instance().createTask(QString("ConcurrentTask_%1").arg(i));
            task->info("Created task");
        }
    }
private:
    int m_count;
};

void CoreLoggingTest::testMultiThreadedTaskCreation()
{
    const int threadCount = 8;
    const int tasksPerThread = 50;

    QThreadPool pool;
    pool.setMaxThreadCount(threadCount);

    for (int i = 0; i < threadCount; ++i) {
        pool.start(new TaskCreatorRunnable(tasksPerThread));
    }
    pool.waitForDone();

    auto allTasks = LogManager::instance().allTasks();
    QCOMPARE(allTasks.size(), threadCount * tasksPerThread);

    QSet<quint64> taskIds;
    for (const auto& task : allTasks) {
        QVERIFY(!taskIds.contains(task->taskId()));
        taskIds.insert(task->taskId());
    }
}

class TaskLoggerRunnable : public QRunnable
{
public:
    TaskLoggerRunnable(std::shared_ptr<TaskContext> task, int logCount)
        : m_task(std::move(task)), m_logCount(logCount) {}

    void run() override
    {
        for (int i = 0; i < m_logCount; ++i) {
            m_task->info(QString("Log message %1 for task %2").arg(i).arg(m_task->taskId()));
        }
    }

private:
    std::shared_ptr<TaskContext> m_task;
    int m_logCount;
};

void CoreLoggingTest::testMultiThreadedLoggingAndIsolation()
{
    auto& manager = LogManager::instance();
    auto taskA = manager.createTask("TaskA");
    auto taskB = manager.createTask("TaskB");

    const int logsPerThread = 100;
    const int threadsPerTask = 4;

    QThreadPool pool;
    pool.setMaxThreadCount(threadsPerTask * 2);

    for (int i = 0; i < threadsPerTask; ++i) {
        pool.start(new TaskLoggerRunnable(taskA, logsPerThread));
        pool.start(new TaskLoggerRunnable(taskB, logsPerThread));
    }
    pool.waitForDone();

    QCOMPARE(taskA->totalEntryCount(), logsPerThread * threadsPerTask);
    QCOMPARE(taskB->totalEntryCount(), logsPerThread * threadsPerTask);

    // Verify task isolation: taskA entries must only have taskA's taskId
    auto entriesA = taskA->allEntries();
    for (const auto& entry : entriesA) {
        QCOMPARE(entry.taskId, taskA->taskId());
    }

    // Verify task isolation: taskB entries must only have taskB's taskId
    auto entriesB = taskB->allEntries();
    for (const auto& entry : entriesB) {
        QCOMPARE(entry.taskId, taskB->taskId());
    }

    // Verify unique sequence across all logged entries
    QSet<quint64> globalSequences;
    for (const auto& entry : entriesA) {
        QVERIFY(!globalSequences.contains(entry.sequence));
        globalSequences.insert(entry.sequence);
    }
    for (const auto& entry : entriesB) {
        QVERIFY(!globalSequences.contains(entry.sequence));
        globalSequences.insert(entry.sequence);
    }
}

QTEST_MAIN(CoreLoggingTest)
#include "tst_CoreLogging.moc"
