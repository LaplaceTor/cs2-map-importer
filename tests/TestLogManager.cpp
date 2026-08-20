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
    void testLogBlockChunkingAndSealing();
    void testExplicitFlushActiveBlock();
    void testLogManagerDefaultThreshold();
    void testLogManagerGetSealedAndAllBlocks();
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

void TestLogManager::testLogBlockChunkingAndSealing()
{
    auto task = LogManager::instance().createTask("Chunking Test Task");
    // Set threshold very small to force multiple blocks
    task->setBlockSizeThreshold(150); // ~150 bytes per block

    const int totalLogs = 100;
    QVector<QString> expectedMessages;
    expectedMessages.reserve(totalLogs);

    for (int i = 0; i < totalLogs; ++i) {
        QString msg = QString("Massive Log Entry Index %1 with extra padding content").arg(i);
        expectedMessages.append(msg);
        task->info(msg);
    }

    // Check that multiple blocks were forced
    QVERIFY(task->sealedBlockCount() > 1);

    auto sealedBlocks = task->sealedBlocks();
    auto allBlocks = task->allBlocks();

    QCOMPARE(allBlocks.size(), sealedBlocks.size() + 1);

    // Verify all sealed blocks are indeed sealed and cannot be appended to
    for (const auto& sealedBlock : sealedBlocks) {
        QVERIFY(sealedBlock.isSealed());
        LogBlock mutableCopy = sealedBlock;
        LogEntry dummyEntry;
        dummyEntry.message = "Attempt append to sealed block";
        QVERIFY(!mutableCopy.append(dummyEntry));
    }

    // Verify block sequence indices are sequential
    quint64 expectedBlockIndex = 0;
    for (const auto& block : allBlocks) {
        QCOMPARE(block.blockIndex(), expectedBlockIndex);
        expectedBlockIndex++;
    }

    // Verify complete log entry count and sequence order across all blocks
    int collectedEntries = 0;
    quint64 expectedSequence = 1;

    for (const auto& block : allBlocks) {
        for (const auto& entry : block.entries()) {
            QCOMPARE(entry.sequence, expectedSequence);
            QCOMPARE(entry.message, expectedMessages[collectedEntries]);
            expectedSequence++;
            collectedEntries++;
        }
    }

    QCOMPARE(collectedEntries, totalLogs);

    // Also verify merged snapshot contains all entries intact
    LogBlock mergedSnapshot = task->logBlockSnapshot();
    QCOMPARE(mergedSnapshot.entryCount(), totalLogs);
    quint64 seq = 1;
    for (int i = 0; i < totalLogs; ++i) {
        QCOMPARE(mergedSnapshot.entries()[i].sequence, seq++);
        QCOMPARE(mergedSnapshot.entries()[i].message, expectedMessages[i]);
    }
}

void TestLogManager::testExplicitFlushActiveBlock()
{
    auto task = LogManager::instance().createTask("Explicit Flush Task");
    task->info("Message 1");
    task->info("Message 2");

    QCOMPARE(task->sealedBlockCount(), static_cast<qsizetype>(0));

    // Explicitly flush
    task->flushActiveBlock();
    QCOMPARE(task->sealedBlockCount(), static_cast<qsizetype>(1));

    QVERIFY(task->sealedBlocks()[0].isSealed());
    QCOMPARE(task->sealedBlocks()[0].entryCount(), static_cast<qsizetype>(2));

    task->info("Message 3");
    task->info("Message 4");

    auto allBlocks = task->allBlocks();
    QCOMPARE(allBlocks.size(), static_cast<qsizetype>(2));

    QCOMPARE(allBlocks[0].entryCount(), static_cast<qsizetype>(2));
    QCOMPARE(allBlocks[1].entryCount(), static_cast<qsizetype>(2));

    QCOMPARE(allBlocks[0].entries()[0].sequence, static_cast<quint64>(1));
    QCOMPARE(allBlocks[0].entries()[1].sequence, static_cast<quint64>(2));
    QCOMPARE(allBlocks[1].entries()[0].sequence, static_cast<quint64>(3));
    QCOMPARE(allBlocks[1].entries()[1].sequence, static_cast<quint64>(4));
}

void TestLogManager::testLogManagerDefaultThreshold()
{
    LogManager::instance().setDefaultBlockSizeThreshold(120);
    QCOMPARE(LogManager::instance().defaultBlockSizeThreshold(), static_cast<qsizetype>(120));

    auto task = LogManager::instance().createTask("Task with Manager Default Threshold");
    QCOMPARE(task->blockSizeThreshold(), static_cast<qsizetype>(120));

    for (int i = 0; i < 10; ++i) {
        task->info(QString("Log message %1").arg(i));
    }

    QVERIFY(task->sealedBlockCount() > 0);
}

void TestLogManager::testLogManagerGetSealedAndAllBlocks()
{
    LogManager::instance().setDefaultBlockSizeThreshold(100);
    auto task = LogManager::instance().createTask("LogManager API Test Task");

    for (int i = 0; i < 15; ++i) {
        task->info(QString("LogManager entry %1 with additional content").arg(i));
    }

    quint64 id = task->taskId();

    QVector<LogBlock> sealedBlocks = LogManager::instance().getSealedBlocks(id);
    QVERIFY(sealedBlocks.size() > 0);
    for (const auto& sb : sealedBlocks) {
        QVERIFY(sb.isSealed());
    }

    bool readSealedSuccess = LogManager::instance().readSealedBlocks(id, [](const QVector<LogBlock>& blocks) {
        QVERIFY(blocks.size() > 0);
        for (const auto& b : blocks) {
            QVERIFY(b.isSealed());
        }
    });
    QVERIFY(readSealedSuccess);

    QVector<LogBlock> allBlocks = LogManager::instance().getAllBlocks(id);
    QCOMPARE(allBlocks.size(), sealedBlocks.size() + 1);

    bool readAllSuccess = LogManager::instance().readAllBlocks(id, [](const QVector<LogBlock>& blocks) {
        QVERIFY(blocks.size() > 0);
    });
    QVERIFY(readAllSuccess);

    bool flushed = LogManager::instance().flushTask(id);
    QVERIFY(flushed);
}

QTEST_MAIN(TestLogManager)
#include "TestLogManager.moc"
