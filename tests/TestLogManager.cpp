#include <QtTest/QtTest>
#include <QThread>
#include <QVector>
#include <atomic>
#include <memory>
#include <set>

#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskState.h"

using namespace Core::Logging;

class TestLogManager : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testBasicTaskCreation();
    void testTaskLifecycleAndStateMachine();
    void testExplicitTaskIdConflict();
    void testMultiThreadTaskCreation();
    void testMultiThreadLogWritingAndIsolation();
    void testLogSequenceOrder();
    void testReentrantReadLogBlock();
};

void TestLogManager::init()
{
    LogManager::instance().clear();
}

void TestLogManager::cleanup()
{
    LogManager::instance().clear();
}

void TestLogManager::testBasicTaskCreation()
{
    auto task1 = LogManager::instance().createTask("Task A");
    QVERIFY(task1 != nullptr);
    QVERIFY(task1->taskId() > 0);
    QCOMPARE(task1->taskName(), QString("Task A"));

    auto task2 = LogManager::instance().createTask("Task B");
    QVERIFY(task2 != nullptr);
    QVERIFY(task2->taskId() != task1->taskId());

    auto foundTask1 = LogManager::instance().findTask(task1->taskId());
    QCOMPARE(foundTask1, task1);

    auto foundTask2 = LogManager::instance().findTask(task2->taskId());
    QCOMPARE(foundTask2, task2);

    auto nonexistent = LogManager::instance().findTask(999999);
    QVERIFY(nonexistent == nullptr);

    QCOMPARE(LogManager::instance().taskCount(), 2);
}

void TestLogManager::testTaskLifecycleAndStateMachine()
{
    auto task1 = LogManager::instance().createTask("Lifecycle 1");
    auto task2 = LogManager::instance().createTask("Lifecycle 2");
    auto task3 = LogManager::instance().createTask("Lifecycle 3");

    QCOMPARE(task1->state(), TaskState::Pending);

    QVERIFY(task1->start());
    QCOMPARE(task1->state(), TaskState::Running);

    bool finished = LogManager::instance().finishTask(task1->taskId(), "Done!");
    QVERIFY(finished);
    QCOMPARE(task1->state(), TaskState::Completed);

    // Terminal state constraints: completed task cannot start, fail, or complete again
    QVERIFY(!task1->start());
    QVERIFY(!task1->complete("Done again"));
    QVERIFY(!task1->fail("Fail completed task"));
    QCOMPARE(task1->state(), TaskState::Completed);

    bool failed = LogManager::instance().failTask(task2->taskId(), "Error occurred");
    QVERIFY(failed);
    QCOMPARE(task2->state(), TaskState::Failed);
    QVERIFY(!task2->start());
    QVERIFY(!task2->complete("Try complete failed task"));

    bool cancelled = LogManager::instance().cancelTask(task3->taskId(), "User cancelled");
    QVERIFY(cancelled);
    QCOMPARE(task3->state(), TaskState::Cancelled);
    QVERIFY(!task3->fail("Try fail cancelled task"));

    QVERIFY(!LogManager::instance().finishTask(888888, "Invalid"));
}

void TestLogManager::testExplicitTaskIdConflict()
{
    auto task1 = LogManager::instance().createTask(100, "Explicit Task 100");
    QVERIFY(task1 != nullptr);
    QCOMPARE(task1->taskId(), static_cast<quint64>(100));

    // Duplicate explicit taskId creation should return nullptr
    auto taskDuplicate = LogManager::instance().createTask(100, "Duplicate Explicit Task 100");
    QVERIFY(taskDuplicate == nullptr);

    // Auto-allocated ID should skip 100
    auto autoTask = LogManager::instance().createTask("Auto Task");
    QVERIFY(autoTask != nullptr);
    QVERIFY(autoTask->taskId() > 100);
}

void TestLogManager::testMultiThreadTaskCreation()
{
    const int threadCount = 10;
    const int tasksPerThread = 20;
    const int totalTasks = threadCount * tasksPerThread;

    QVector<QThread*> threads;
    threads.reserve(threadCount);

    for (int t = 0; t < threadCount; ++t) {
        threads.append(QThread::create([t, tasksPerThread]() {
            for (int i = 0; i < tasksPerThread; ++i) {
                QString name = QString("Thread_%1_Task_%2").arg(t).arg(i);
                LogManager::instance().createTask(name);
            }
        }));
    }

    for (auto* thread : threads) {
        thread->start();
    }

    for (auto* thread : threads) {
        thread->wait();
        delete thread;
    }

    QCOMPARE(LogManager::instance().taskCount(), totalTasks);

    auto ids = LogManager::instance().taskIds();
    std::set<quint64> uniqueIds(ids.begin(), ids.end());
    QCOMPARE(static_cast<int>(uniqueIds.size()), totalTasks);
}

void TestLogManager::testMultiThreadLogWritingAndIsolation()
{
    const int taskCount = 5;
    const int threadsPerTask = 4;
    const int logsPerThread = 50;

    QVector<std::shared_ptr<TaskLoggingContext>> tasks;
    for (int i = 0; i < taskCount; ++i) {
        tasks.append(LogManager::instance().createTask(QString("Task_%1").arg(i)));
    }

    QVector<QThread*> threads;

    for (int taskIdx = 0; taskIdx < taskCount; ++taskIdx) {
        auto task = tasks[taskIdx];
        quint64 expectedTaskId = task->taskId();

        for (int t = 0; t < threadsPerTask; ++t) {
            threads.append(QThread::create([task, expectedTaskId, t, logsPerThread]() {
                for (int i = 0; i < logsPerThread; ++i) {
                    task->info(QString("Task %1 Thread %2 Log %3").arg(expectedTaskId).arg(t).arg(i));
                }
            }));
        }
    }

    for (auto* thread : threads) {
        thread->start();
    }

    for (auto* thread : threads) {
        thread->wait();
        delete thread;
    }

    // Verify isolation and entry counts
    const int expectedEntriesPerTask = threadsPerTask * logsPerThread;

    for (int i = 0; i < taskCount; ++i) {
        auto task = tasks[i];
        quint64 taskId = task->taskId();

        // Zero-copy read test
        bool readSuccess = LogManager::instance().readLogBlock(taskId, [taskId, expectedEntriesPerTask](const LogBlock& block) {
            QCOMPARE(block.entryCount(), expectedEntriesPerTask);
            const auto& entries = block.entries();
            for (const auto& entry : entries) {
                QVERIFY(entry.message.startsWith(QString("Task %1 ").arg(taskId)));
            }
        });
        QVERIFY(readSuccess);

        // Snapshot copy test
        LogBlock snapshot = LogManager::instance().getLogBlockSnapshot(taskId);
        QCOMPARE(snapshot.entryCount(), expectedEntriesPerTask);
    }
}

void TestLogManager::testLogSequenceOrder()
{
    auto task = LogManager::instance().createTask("Order Test Task");

    const int threadCount = 8;
    const int logsPerThread = 100;
    const int totalLogs = threadCount * logsPerThread;

    QVector<QThread*> threads;
    for (int t = 0; t < threadCount; ++t) {
        threads.append(QThread::create([task, t, logsPerThread]() {
            for (int i = 0; i < logsPerThread; ++i) {
                task->info(QString("Thread %1 Msg %2").arg(t).arg(i));
            }
        }));
    }

    for (auto* thread : threads) {
        thread->start();
    }

    for (auto* thread : threads) {
        thread->wait();
        delete thread;
    }

    bool readSuccess = LogManager::instance().readLogBlock(task->taskId(), [totalLogs](const LogBlock& block) {
        QCOMPARE(block.entryCount(), totalLogs);

        const auto& entries = block.entries();
        quint64 expectedSequence = 1;
        for (const auto& entry : entries) {
            QCOMPARE(entry.sequence, expectedSequence);
            expectedSequence++;
        }
    });
    QVERIFY(readSuccess);
}

void TestLogManager::testReentrantReadLogBlock()
{
    auto task = LogManager::instance().createTask("Reentrant Test Task");
    task->info("Initial Entry");

    bool readSuccess = LogManager::instance().readLogBlock(task->taskId(), [task](const LogBlock& block) {
        QCOMPARE(block.entryCount(), 1);
        // Re-entrant logging inside read callback must not deadlock
        task->info("Re-entrant Entry");
    });
    QVERIFY(readSuccess);

    LogBlock snapshot = task->logBlockSnapshot();
    QCOMPARE(snapshot.entryCount(), 2);
}

QTEST_MAIN(TestLogManager)
#include "TestLogManager.moc"
