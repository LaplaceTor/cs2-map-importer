#include <QtTest/QtTest>
#include <QThread>
#include <QTemporaryDir>
#include <QVector>
#include <atomic>
#include <memory>
#include <set>

#include "Core/Logging/FileSink.h"
#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskState.h"

using namespace Core::Logging;

class FailingMockSink : public ILogSink {
public:
    int failAfterBlocks = 1;
    int writtenBlocks = 0;
    bool shouldFailFlush = false;

    bool writeBlock(const LogBlock& block, const QString& taskName) override
    {
        Q_UNUSED(block);
        Q_UNUSED(taskName);
        if (writtenBlocks >= failAfterBlocks) {
            return false;
        }
        writtenBlocks++;
        return true;
    }

    bool flush() override
    {
        return !shouldFailFlush;
    }
};

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
    void testTaskCompletionAutoSealsFinalBlock();
    void testFileSinkMultiTaskAndBlocks();
    void testConcurrentFlushTasks();
    void testDynamicAddSinkHistory();
    void testSinkErrorAndCursorRollback();
    void testFlushFailureRetryAndCursorRollback();
    void testFlushTaskReturnValueOnFailure();
    void testFinishTaskReturnValueOnFlushFailure();
    void testFileSinkAtomicWriteBlock();
    void testSinkIdPointerReuseSafety();
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

    QCOMPARE(allBlocks.size(), sealedBlocks.size());

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

    // Finish task and verify final block auto-seals
    task->complete("Task Done");
    QCOMPARE(task->sealedBlockCount(), task->totalBlockCount());
    for (const auto& sb : task->sealedBlocks()) {
        QVERIFY(sb.isSealed());
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
    QCOMPARE(allBlocks.size(), sealedBlocks.size());

    bool readAllSuccess = LogManager::instance().readAllBlocks(id, [](const QVector<LogBlock>& blocks) {
        QVERIFY(blocks.size() > 0);
    });
    QVERIFY(readAllSuccess);

    bool flushed = LogManager::instance().flushTask(id);
    QVERIFY(flushed);
}

void TestLogManager::testTaskCompletionAutoSealsFinalBlock()
{
    auto task1 = LogManager::instance().createTask("Complete Task Test");
    task1->setBlockSizeThreshold(0); // Unlimited threshold for explicit test
    task1->info("Entry 1");
    task1->info("Entry 2");
    QCOMPARE(task1->sealedBlockCount(), static_cast<qsizetype>(0));

    task1->complete("Finished successfully");
    QCOMPARE(task1->sealedBlockCount(), static_cast<qsizetype>(1));
    QCOMPARE(task1->totalBlockCount(), static_cast<qsizetype>(1));
    QCOMPARE(task1->allBlocks().size(), static_cast<qsizetype>(1));
    QVERIFY(task1->sealedBlocks()[0].isSealed());
    QCOMPARE(task1->sealedBlocks()[0].entryCount(), static_cast<qsizetype>(3)); // Entry 1, Entry 2, Complete msg

    auto task2 = LogManager::instance().createTask("Fail Task Test");
    task2->info("Entry A");
    task2->fail("Failed with error");
    QCOMPARE(task2->sealedBlockCount(), static_cast<qsizetype>(1));
    QCOMPARE(task2->totalBlockCount(), static_cast<qsizetype>(1));
    QVERIFY(task2->sealedBlocks()[0].isSealed());

    auto task3 = LogManager::instance().createTask("Cancel Task Test");
    task3->info("Entry X");
    task3->cancel("Cancelled by user");
    QCOMPARE(task3->sealedBlockCount(), static_cast<qsizetype>(1));
    QCOMPARE(task3->totalBlockCount(), static_cast<qsizetype>(1));
    QVERIFY(task3->sealedBlocks()[0].isSealed());
}

void TestLogManager::testFileSinkMultiTaskAndBlocks()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString logFilePath = tempDir.path() + "/test_output.log";

    auto sink = std::make_shared<FileSink>(logFilePath);
    QVERIFY(sink->isOpen());
    LogManager::instance().addSink(sink);

    auto task1 = LogManager::instance().createTask("Task Alpha");
    auto task2 = LogManager::instance().createTask("Task Beta");

    // Force small block size to produce multiple blocks for each task
    task1->setBlockSizeThreshold(120);
    task2->setBlockSizeThreshold(120);

    const int entriesPerTask = 30;

    for (int i = 0; i < entriesPerTask; ++i) {
        task1->info(QString("Alpha Log Entry %1").arg(i));
        task2->warning(QString("Beta Log Entry %1").arg(i));
    }

    // Verify multiple sealed blocks generated for both tasks
    QVERIFY(task1->sealedBlockCount() > 1);
    QVERIFY(task2->sealedBlockCount() > 1);

    // Flush and finish tasks
    LogManager::instance().finishTask(task1->taskId(), "Alpha Complete");
    LogManager::instance().finishTask(task2->taskId(), "Beta Complete");

    const int expectedAlphaTotal = entriesPerTask + 1;
    const int expectedBetaTotal = entriesPerTask + 1;
    const int expectedTotalLines = expectedAlphaTotal + expectedBetaTotal;

    sink->close();

    QFile file(logFilePath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&file);

    QStringList lines;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }

    QCOMPARE(lines.size(), expectedTotalLines);

    int alphaIndex = 0;
    int betaIndex = 0;

    QString alphaIdStr = QString("Task %1").arg(task1->taskId());
    QString betaIdStr = QString("Task %2").arg(task2->taskId());

    for (const QString& line : lines) {
        // Format check: [timestamp] [Task N - Name] [Block B] [Seq S] [LEVEL] message
        QVERIFY(line.startsWith("["));
        QVERIFY(line.contains("] [Block "));
        QVERIFY(line.contains("] [Seq "));

        if (line.contains(alphaIdStr)) {
            QVERIFY(line.contains("Task Alpha"));
            if (alphaIndex < entriesPerTask) {
                QVERIFY(line.contains(QString("Alpha Log Entry %1").arg(alphaIndex)));
                QVERIFY(line.contains("[INFO]"));
            } else {
                QVERIFY(line.contains("Alpha Complete"));
            }
            QVERIFY(!line.contains("Beta"));
            alphaIndex++;
        } else if (line.contains(betaIdStr)) {
            QVERIFY(line.contains("Task Beta"));
            if (betaIndex < entriesPerTask) {
                QVERIFY(line.contains(QString("Beta Log Entry %1").arg(betaIndex)));
                QVERIFY(line.contains("[WARNING]"));
            } else {
                QVERIFY(line.contains("Beta Complete"));
            }
            QVERIFY(!line.contains("Alpha"));
            betaIndex++;
        } else {
            QFAIL("Line belongs to neither Task Alpha nor Task Beta");
        }
    }

    QCOMPARE(alphaIndex, expectedAlphaTotal);
    QCOMPARE(betaIndex, expectedBetaTotal);
}

void TestLogManager::testConcurrentFlushTasks()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString logFilePath = tempDir.path() + "/concurrent_flush.log";

    auto sink = std::make_shared<FileSink>(logFilePath);
    LogManager::instance().addSink(sink);

    const int taskCount = 10;
    const int threadsPerTask = 3;
    const int logsPerThread = 50;

    QVector<std::shared_ptr<TaskLoggingContext>> tasks;
    for (int i = 0; i < taskCount; ++i) {
        auto t = LogManager::instance().createTask(QString("ConcurrentTask_%1").arg(i));
        t->setBlockSizeThreshold(200); // generate multiple blocks
        tasks.append(t);
    }

    QVector<QThread*> threads;
    for (int taskIdx = 0; taskIdx < taskCount; ++taskIdx) {
        auto task = tasks[taskIdx];
        quint64 taskId = task->taskId();

        for (int th = 0; th < threadsPerTask; ++th) {
            threads.append(QThread::create([task, taskId, th, logsPerThread]() {
                for (int i = 0; i < logsPerThread; ++i) {
                    task->info(QString("Task %1 Th %2 Log %3").arg(taskId).arg(th).arg(i));
                    if (i % 10 == 0) {
                        LogManager::instance().flushTask(taskId);
                    }
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

    LogManager::instance().flushAll();
    sink->close();

    QFile file(logFilePath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&file);

    int totalLines = 0;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            totalLines++;
        }
    }

    const int expectedTotalLogs = taskCount * threadsPerTask * logsPerThread;
    QCOMPARE(totalLines, expectedTotalLogs);
}

void TestLogManager::testDynamicAddSinkHistory()
{
    auto task = LogManager::instance().createTask("History Task");
    task->setBlockSizeThreshold(100);

    for (int i = 0; i < 20; ++i) {
        task->info(QString("History Log Entry %1").arg(i));
    }
    QVERIFY(task->sealedBlockCount() > 0);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString logFilePath = tempDir.path() + "/late_sink.log";

    // Add sink after task produced sealed blocks
    auto lateSink = std::make_shared<FileSink>(logFilePath);
    LogManager::instance().addSink(lateSink);

    // Flush task to lateSink
    LogManager::instance().finishTask(task->taskId(), "Task Finished");
    lateSink->close();

    QFile file(logFilePath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&file);

    int lineCount = 0;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            lineCount++;
        }
    }

    // 20 entries + 1 finish message = 21 lines in late-added sink
    QCOMPARE(lineCount, 21);
}

void TestLogManager::testSinkErrorAndCursorRollback()
{
    auto task = LogManager::instance().createTask("Rollback Task");
    task->setBlockSizeThreshold(50); // small threshold to force multiple sealed blocks

    for (int i = 0; i < 10; ++i) {
        task->info(QString("Rollback Entry %1").arg(i));
    }
    task->flushActiveBlock();
    QVERIFY(task->sealedBlockCount() >= 3);

    auto mockSink = std::make_shared<FailingMockSink>();
    mockSink->failAfterBlocks = 1; // Only allow 1 block to succeed
    LogManager::instance().addSink(mockSink);

    // Flush task: 1st block succeeds, 2nd block fails, cursor rolls back
    bool ok = LogManager::instance().flushTask(task->taskId());
    QVERIFY(!ok);
    QCOMPARE(mockSink->writtenBlocks, 1);

    // Fix sink failure state
    mockSink->failAfterBlocks = 100;

    // Retry flush: should resume from block 1 (since block 0 was committed, block 1+ rolled back)
    ok = LogManager::instance().flushTask(task->taskId());
    QVERIFY(ok);
    QCOMPARE(mockSink->writtenBlocks, static_cast<int>(task->sealedBlockCount()));
}

void TestLogManager::testFlushFailureRetryAndCursorRollback()
{
    auto task = LogManager::instance().createTask("Flush Failure Task");
    task->info("Msg 0");
    task->info("Msg 1");
    task->flushActiveBlock();

    auto mockSink = std::make_shared<FailingMockSink>();
    mockSink->failAfterBlocks = 100;
    mockSink->shouldFailFlush = true; // writeBlock succeeds, flush fails
    LogManager::instance().addSink(mockSink);

    bool ok = LogManager::instance().flushTask(task->taskId());
    QVERIFY(!ok);
    QCOMPARE(mockSink->writtenBlocks, 1);

    // Fix flush state and retry
    mockSink->shouldFailFlush = false;
    ok = LogManager::instance().flushTask(task->taskId());
    QVERIFY(ok);
    // Should re-attempt writing the block because it was not committed due to flush failure
    QCOMPARE(mockSink->writtenBlocks, 2);
}

void TestLogManager::testFinishTaskReturnValueOnFlushFailure()
{
    auto task = LogManager::instance().createTask("Finish Failure Task");
    task->info("Message");

    auto mockSink = std::make_shared<FailingMockSink>();
    mockSink->failAfterBlocks = 0; // writeBlock fails
    LogManager::instance().addSink(mockSink);

    bool finishOk = LogManager::instance().finishTask(task->taskId(), "Done");
    QVERIFY(!finishOk);
}

void TestLogManager::testFlushTaskReturnValueOnFailure()
{
    auto task = LogManager::instance().createTask("Flush Return Task");
    task->info("Message");
    task->flushActiveBlock();

    auto mockSink = std::make_shared<FailingMockSink>();
    mockSink->failAfterBlocks = 0; // writeBlock fails
    LogManager::instance().addSink(mockSink);

    bool result = LogManager::instance().flushTask(task->taskId());
    QVERIFY(!result);
}

void TestLogManager::testFileSinkAtomicWriteBlock()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString logFilePath = tempDir.path() + "/atomic_test.log";

    FileSink sink(logFilePath);
    QVERIFY(sink.isOpen());

    LogBlock block(1, 0);
    LogEntry e1;
    e1.sequence = 1;
    e1.timestamp = 1000;
    e1.level = LogLevel::Info;
    e1.message = "Line 1";

    LogEntry e2;
    e2.sequence = 2;
    e2.timestamp = 1001;
    e2.level = LogLevel::Error;
    e2.message = "Line 2";

    block.append(e1);
    block.append(e2);

    bool writeOk = sink.writeBlock(block, "AtomicTask");
    QVERIFY(writeOk);
    QVERIFY(sink.flush());

    QFile file(logFilePath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains("Line 1"));
    QVERIFY(content.contains("Line 2"));
}

void TestLogManager::testSinkIdPointerReuseSafety()
{
    auto sink1 = std::make_shared<FailingMockSink>();
    auto sink2 = std::make_shared<FailingMockSink>();

    QVERIFY(sink1->sinkId() != sink2->sinkId());

    LogManager::instance().addSink(sink1);
    LogManager::instance().removeSink(sink1);
    LogManager::instance().addSink(sink2);

    // Both sinks had independent IDs, sink2 starts fresh with cursor 0
    auto task = LogManager::instance().createTask("ID Safety Task");
    task->info("Message 1");
    LogManager::instance().flushTask(task->taskId());

    QCOMPARE(sink2->writtenBlocks, 1);
}

QTEST_MAIN(TestLogManager)
#include "TestLogManager.moc"
