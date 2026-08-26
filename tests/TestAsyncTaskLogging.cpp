#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QThreadPool>
#include <QPointer>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>

#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Logging/LogLevel.h"
#include "Core/Logging/TaskState.h"
#include "Application/Async/AsyncTaskRunner.h"
#include "UI/ViewModels/LogViewModel.h"

using namespace Core::Logging;
using namespace Application::Async;
using namespace UI::ViewModels;

class TestAsyncTaskLogging : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testSingleTaskLifecycle();
    void testTaskFailure();
    void testTaskCancellation();
    void testConcurrentTasksIsolation();
    void testMultiBlockTask();
    void testAsyncTaskRunnerNormal();
    void testAsyncTaskRunnerExceptionSafety();
    void testContextDestroyedSafety();
    void testCopyAllFormat();
    void testExpandCollapseState();
    void testHierarchicalSubModelGranularity();
    void testMultiLevelNestedTasksAndParallelChildExecution();
};

void TestAsyncTaskLogging::init()
{
    LogManager::instance().clear();
}

void TestAsyncTaskLogging::cleanup()
{
    LogManager::instance().clear();
}

void TestAsyncTaskLogging::testSingleTaskLifecycle()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    auto task = LogManager::instance().createTask("Detect Environment");
    QVERIFY(task != nullptr);
    task->start();

    task->info("Starting detection");
    task->info("Found 2 installations");

    LogManager::instance().finishTask(task->taskId(), "Detection finished");

    // Wait for queued delivery to LogViewModel
    QTRY_COMPARE(logVm->taskCount(), 1);
    QTRY_COMPARE(logVm->totalMessageCount(), 3);

    QModelIndex idx = logVm->index(0, 0);
    QCOMPARE(logVm->data(idx, LogViewModel::TaskNameRole).toString(), QStringLiteral("Detect Environment"));
    QCOMPARE(logVm->data(idx, LogViewModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));
    QCOMPARE(logVm->data(idx, LogViewModel::MessageCountRole).toInt(), 3);

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testTaskFailure()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    auto task = LogManager::instance().createTask("Validate Source 1");
    QVERIFY(task != nullptr);
    task->start();

    task->info("Checking gameinfo.txt");
    task->error("gameinfo.txt not found");

    LogManager::instance().failTask(task->taskId(), "Validation failed");

    QTRY_COMPARE(logVm->taskCount(), 1);
    QTRY_COMPARE(logVm->totalMessageCount(), 3);

    QModelIndex idx = logVm->index(0, 0);
    QCOMPARE(logVm->data(idx, LogViewModel::TaskNameRole).toString(), QStringLiteral("Validate Source 1"));
    QCOMPARE(logVm->data(idx, LogViewModel::StateStringRole).toString(), QStringLiteral("FAILED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testTaskCancellation()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    auto task = LogManager::instance().createTask("Import Map");
    QVERIFY(task != nullptr);
    task->start();

    task->info("Compiling assets...");
    LogManager::instance().cancelTask(task->taskId(), "User cancelled import");

    QTRY_COMPARE(logVm->taskCount(), 1);
    QTRY_COMPARE(logVm->totalMessageCount(), 2);

    QModelIndex idx = logVm->index(0, 0);
    QCOMPARE(logVm->data(idx, LogViewModel::TaskNameRole).toString(), QStringLiteral("Import Map"));
    QCOMPARE(logVm->data(idx, LogViewModel::StateStringRole).toString(), QStringLiteral("CANCELLED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testConcurrentTasksIsolation()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    const int taskCount = 4;
    const int entriesPerTask = 50;

    std::vector<std::shared_ptr<TaskLoggingContext>> tasks;
    for (int i = 0; i < taskCount; ++i) {
        auto t = LogManager::instance().createTask(QStringLiteral("Concurrent Task %1").arg(i + 1));
        t->start();
        tasks.push_back(t);
    }

    std::atomic<bool> startFlag{false};
    std::vector<std::thread> workers;

    for (int i = 0; i < taskCount; ++i) {
        workers.emplace_back([t = tasks[i], i, entriesPerTask, &startFlag]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }
            for (int e = 0; e < entriesPerTask; ++e) {
                t->info(QStringLiteral("Task %1 entry %2").arg(i + 1).arg(e + 1));
                if (e % 10 == 0) {
                    LogManager::instance().flushTask(t->taskId());
                }
            }
            LogManager::instance().finishTask(t->taskId(), QStringLiteral("Task %1 done").arg(i + 1));
        });
    }

    startFlag.store(true);
    for (auto& w : workers) {
        w.join();
    }

    QTRY_COMPARE_WITH_TIMEOUT(logVm->taskCount(), taskCount, 5000);
    // entriesPerTask + 1 (for finishTask message) per task
    int expectedTotal = taskCount * (entriesPerTask + 1);
    QTRY_COMPARE_WITH_TIMEOUT(logVm->totalMessageCount(), expectedTotal, 5000);

    // Verify isolation: check each task by name
    for (int i = 0; i < taskCount; ++i) {
        QString expectedName = QStringLiteral("Concurrent Task %1").arg(i + 1);
        int foundRow = -1;
        for (int r = 0; r < logVm->taskCount(); ++r) {
            QModelIndex rIdx = logVm->index(r, 0);
            if (logVm->data(rIdx, LogViewModel::TaskNameRole).toString() == expectedName) {
                foundRow = r;
                break;
            }
        }
        QVERIFY2(foundRow != -1, qPrintable(QStringLiteral("Task not found: %1").arg(expectedName)));

        QModelIndex idx = logVm->index(foundRow, 0);
        QCOMPARE(logVm->data(idx, LogViewModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));
        QCOMPARE(logVm->data(idx, LogViewModel::MessageCountRole).toInt(), entriesPerTask + 1);

        QVariantList msgs = logVm->data(idx, LogViewModel::MessagesRole).toList();
        // Verify NO logs from other tasks leaked into this task's messages
        for (const auto& msgVar : msgs) {
            QString msgText = msgVar.toMap()[QStringLiteral("message")].toString();
            for (int other = 0; other < taskCount; ++other) {
                if (other != i) {
                    QVERIFY(!msgText.contains(QStringLiteral("Task %1 entry").arg(other + 1)));
                }
            }
        }
    }

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testMultiBlockTask()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    auto task = LogManager::instance().createTask("MultiBlock Task");
    task->start();

    // Block 1
    task->info("Step 1");
    LogManager::instance().flushTask(task->taskId());

    // Block 2
    task->info("Step 2");
    LogManager::instance().flushTask(task->taskId());

    // Block 3
    task->info("Step 3");
    LogManager::instance().finishTask(task->taskId(), "Final step");

    QTRY_COMPARE(logVm->taskCount(), 1);
    QTRY_COMPARE(logVm->totalMessageCount(), 4);

    QModelIndex idx = logVm->index(0, 0);
    QCOMPARE(logVm->data(idx, LogViewModel::MessageCountRole).toInt(), 4);

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testAsyncTaskRunnerNormal()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    bool callbackInvoked = false;
    int returnedResult = 0;

    AsyncTaskRunner::run<int>(
        QStringLiteral("Runner Task"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> int {
            if (ctx) {
                ctx->info("Runner working...");
            }
            return 42;
        },
        [&](int val) {
            callbackInvoked = true;
            returnedResult = val;
        });

    QTRY_VERIFY(callbackInvoked);
    QCOMPARE(returnedResult, 42);

    QTRY_COMPARE(logVm->taskCount(), 1);
    QModelIndex idx = logVm->index(0, 0);
    QCOMPARE(logVm->data(idx, LogViewModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testAsyncTaskRunnerExceptionSafety()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    bool callbackInvoked = false;
    int returnedResult = 999;

    AsyncTaskRunner::run<int>(
        QStringLiteral("Faulty Runner Task"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> int {
            if (ctx) {
                ctx->info("About to throw");
            }
            throw std::runtime_error("Simulated I/O failure");
        },
        [&](int val) {
            callbackInvoked = true;
            returnedResult = val;
        });

    QTRY_VERIFY(callbackInvoked);
    QCOMPARE(returnedResult, 0); // Default constructed result on exception

    QTRY_COMPARE(logVm->taskCount(), 1);
    QModelIndex idx = logVm->index(0, 0);
    QCOMPARE(logVm->data(idx, LogViewModel::StateStringRole).toString(), QStringLiteral("FAILED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testContextDestroyedSafety()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    auto* tempContext = new QObject();
    std::atomic<bool> workerRan{false};

    AsyncTaskRunner::run<int>(
        QStringLiteral("Destroyed Context Task"),
        tempContext,
        [&workerRan](std::shared_ptr<TaskLoggingContext> ctx) -> int {
            if (ctx) {
                ctx->info("Worker executing while context is deleted");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            workerRan.store(true);
            return 100;
        },
        [](int) {
            QFAIL("Callback should not be called when context is destroyed");
        });

    // Delete context immediately while worker is running
    delete tempContext;

    QTRY_VERIFY_WITH_TIMEOUT(workerRan.load(), 2000);
    QTRY_COMPARE(logVm->taskCount(), 1);

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testCopyAllFormat()
{
    LogViewModel logVm;
    auto task = LogManager::instance().createTask("Validate Source 2");
    task->start();
    task->info("Checking gameinfo.gi");
    task->warning("Optional addon missing");
    LogManager::instance().finishTask(task->taskId(), "Validation completed");

    // Feed to logVm
    logVm.processIncomingBlock(task->sealedBlocks().first(), task->taskName());
    QCoreApplication::processEvents();

    QString logText = logVm.getFullLogText();
    QVERIFY(logText.contains(QStringLiteral("=== Validate Source 2 ===")));
    QVERIFY(logText.contains(QStringLiteral("INFO   Checking gameinfo.gi")));
    QVERIFY(logText.contains(QStringLiteral("WARN   Optional addon missing")));
    QVERIFY(logText.contains(QStringLiteral("INFO   Validation completed")));
}

void TestAsyncTaskLogging::testExpandCollapseState()
{
    LogViewModel logVm;
    logVm.appendLog("Line 1", 1);
    logVm.appendLog("Line 2", 2);
    QCoreApplication::processEvents();

    QCOMPARE(logVm.taskCount(), 1);
    QModelIndex idx = logVm.index(0, 0);
    QVERIFY(logVm.data(idx, LogViewModel::ExpandedRole).toBool());

    logVm.collapseAll();
    QVERIFY(!logVm.data(idx, LogViewModel::ExpandedRole).toBool());

    logVm.expandAll();
    QVERIFY(logVm.data(idx, LogViewModel::ExpandedRole).toBool());

    logVm.toggleTaskExpanded(0);
    QVERIFY(!logVm.data(idx, LogViewModel::ExpandedRole).toBool());
}

void TestAsyncTaskLogging::testHierarchicalSubModelGranularity()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    auto task = LogManager::instance().createTask("Granular SubModel Task");
    task->start();
    task->info("Message 1");
    task->warning("Message 2");
    LogManager::instance().flushTask(task->taskId());

    QTRY_COMPARE(logVm->taskCount(), 1);

    auto* subModel = logVm->getTaskMessagesModel(0);
    QVERIFY(subModel != nullptr);
    QCOMPARE(subModel->rowCount(), 2);

    QSignalSpy insertSpy(subModel, &QAbstractItemModel::rowsInserted);

    task->error("Message 3");
    LogManager::instance().finishTask(task->taskId(), "Done");

    QTRY_COMPARE(subModel->rowCount(), 4);
    QVERIFY(insertSpy.count() >= 1);

    QModelIndex mIdx0 = subModel->index(0, 0);
    QCOMPARE(subModel->data(mIdx0, LogMessageListModel::MessageRole).toString(), QStringLiteral("Message 1"));
    QCOMPARE(subModel->data(mIdx0, LogMessageListModel::LevelStringRole).toString(), QStringLiteral("INFO"));

    QModelIndex mIdx2 = subModel->index(2, 0);
    QCOMPARE(subModel->data(mIdx2, LogMessageListModel::MessageRole).toString(), QStringLiteral("Message 3"));
    QCOMPARE(subModel->data(mIdx2, LogMessageListModel::LevelStringRole).toString(), QStringLiteral("ERROR"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testMultiLevelNestedTasksAndParallelChildExecution()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    // 1. Create Root Task
    auto rootTask = LogManager::instance().createTask("Root Map Import");
    rootTask->start();
    rootTask->info("Starting root pipeline");
    LogManager::instance().flushTask(rootTask->taskId());

    QTRY_COMPARE(logVm->taskCount(), 1);

    // 2. Launch 3 parallel child tasks attached to rootTask->taskId()
    std::atomic<int> completedChildren{0};
    const int childCount = 3;

    for (int i = 0; i < childCount; ++i) {
        AsyncTaskRunner::runChild<int>(
            rootTask->taskId(),
            QStringLiteral("Parallel Child Task %1").arg(i + 1),
            this,
            [i](std::shared_ptr<TaskLoggingContext> ctx) -> int {
                if (ctx) {
                    ctx->info(QStringLiteral("Child %1 working in parallel").arg(i + 1));
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    ctx->info(QStringLiteral("Child %1 finished work").arg(i + 1));
                }
                return i + 1;
            },
            [&completedChildren](int) {
                completedChildren.fetch_add(1);
            });
    }

    QTRY_COMPARE_WITH_TIMEOUT(completedChildren.load(), childCount, 5000);

    // Verify root task subTasksModel has all 3 children
    QModelIndex rootIdx = logVm->index(0, 0);
    QCOMPARE(logVm->data(rootIdx, LogTaskModel::DepthRole).toInt(), 0);
    QCOMPARE(logVm->data(rootIdx, LogTaskModel::HasSubTasksRole).toBool(), true);
    QCOMPARE(logVm->data(rootIdx, LogTaskModel::SubTasksCountRole).toInt(), childCount);

    auto* subTasksModel = logVm->getTaskSubTasksModel(0);
    QVERIFY(subTasksModel != nullptr);
    QCOMPARE(subTasksModel->taskCount(), childCount);

    for (int c = 0; c < childCount; ++c) {
        QModelIndex childIdx = subTasksModel->index(c, 0);
        QCOMPARE(subTasksModel->data(childIdx, LogTaskModel::DepthRole).toInt(), 1);
        QCOMPARE(subTasksModel->data(childIdx, LogTaskModel::ParentTaskIdRole).toULongLong(), rootTask->taskId());
        QCOMPARE(subTasksModel->data(childIdx, LogTaskModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));
        QVERIFY(subTasksModel->data(childIdx, LogTaskModel::MessageCountRole).toInt() >= 2);
    }

    LogManager::instance().finishTask(rootTask->taskId(), "Root pipeline done");

    // Test recursive formatted log text
    QString fullText = logVm->getFullLogText();
    QVERIFY(fullText.contains(QStringLiteral("=== Root Map Import ===")));
    QVERIFY(fullText.contains(QStringLiteral("--- Parallel Child Task 1 ---")));
    QVERIFY(fullText.contains(QStringLiteral("--- Parallel Child Task 2 ---")));
    QVERIFY(fullText.contains(QStringLiteral("--- Parallel Child Task 3 ---")));

    logVm->unregisterFromLogManager();
}

QTEST_MAIN(TestAsyncTaskLogging)
#include "TestAsyncTaskLogging.moc"
