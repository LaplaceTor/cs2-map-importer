#include <QTest>
#include <QSignalSpy>
#include <QAbstractItemModelTester>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Logging/TaskState.h"
#include "Core/Async/TaskResult.h"
#include "UI/ViewModels/LogViewModel.h"
#include "UI/ViewModels/LogTaskModel.h"
#include "Application/Async/AsyncTaskRunner.h"

using namespace Core::Logging;
using namespace Core::Async;
using namespace UI::ViewModels;
using namespace Application::Async;

class TestAsyncTaskLogging : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Unit tests for LogManager + TaskLoggingContext lifecycle & state transitions
    void testSingleTaskLifecycle();
    void testTaskFailure();
    void testTaskCancellation();
    void testConcurrentTasksIsolation();
    void testMultiBlockTask();

    // AsyncTaskRunner async lifecycle & UI context delivery tests
    void testAsyncTaskRunnerNormal();
    void testAsyncTaskRunnerExceptionSafety();
    void testContextDestroyedSafety();
    void testCopyAllFormat();
    void testExpandCollapseState();
    void testHierarchicalSubModelGranularity();
    void testMultiLevelNestedTasksAndParallelChildExecution();
    void testNullContextAndEmptyCallbackExecution();
    void testSemanticBusinessFailureDetection();

    // TaskResult outcome mapping tests
    void testTaskResultOutcomes();
    void testInvalidParentTaskRejection();
    void testLoggedErrorForcesTaskFailure();
    void testLoggedWarningPreservesTaskCompleted();
    void testRunTaskAndChildTaskApi();

    // State contradiction and contract violation reconciliation tests
    void testExplicitFailWithTaskResultSuccessContradiction();
    void testExplicitCancelWithTaskResultSuccessContradiction();
    void testExplicitSkipWithTaskResultSuccessContradiction();
    void testExplicitCompleteWithTaskResultFailureContradiction();
};

void TestAsyncTaskLogging::initTestCase()
{
}

void TestAsyncTaskLogging::cleanupTestCase()
{
}

void TestAsyncTaskLogging::init()
{
    LogManager::instance().clear();
}

void TestAsyncTaskLogging::cleanup()
{
    QThreadPool::globalInstance()->waitForDone(3000);
    LogManager::instance().clear();
}

void TestAsyncTaskLogging::testSingleTaskLifecycle()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    QCOMPARE(logVm->taskCount(), 0);

    // 1. Create and start task
    auto task = LogManager::instance().createTask("Map Importer Pipeline");
    QVERIFY(task != nullptr);
    task->start();

    // 2. Emit logs and flush
    task->info("Extracting VPK files...");
    task->debug("Resolving search paths: csgo/pak01_dir.vpk");
    LogManager::instance().flushTask(task->taskId());

    QTRY_COMPARE(logVm->taskCount(), 1);

    QModelIndex taskIdx = logVm->index(0, 0);
    QCOMPARE(logVm->data(taskIdx, LogTaskModel::TaskNameRole).toString(), QStringLiteral("Map Importer Pipeline"));
    QCOMPARE(logVm->data(taskIdx, LogTaskModel::StateStringRole).toString(), QStringLiteral("RUNNING"));
    QCOMPARE(logVm->data(taskIdx, LogTaskModel::MessageCountRole).toInt(), 2);

    auto* msgModel = logVm->getTaskMessagesModel(0);
    QVERIFY(msgModel != nullptr);
    QCOMPARE(msgModel->rowCount(), 2);

    QModelIndex msg0 = msgModel->index(0, 0);
    QCOMPARE(msgModel->data(msg0, LogMessageListModel::LevelStringRole).toString(), QStringLiteral("INFO"));
    QCOMPARE(msgModel->data(msg0, LogMessageListModel::MessageRole).toString(), QStringLiteral("Extracting VPK files..."));

    // 3. Complete task
    LogManager::instance().finishTask(task->taskId(), "Map successfully imported");
    QTRY_COMPARE(logVm->data(taskIdx, LogTaskModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testTaskFailure()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    auto task = LogManager::instance().createTask("Compile VMF");
    task->start();
    task->info("Invoking resourcecompiler.exe");
    task->error("Fatal: out of memory while compiling mesh");
    LogManager::instance().flushTask(task->taskId());

    QTRY_COMPARE(logVm->taskCount(), 1);
    auto* msgModel = logVm->getTaskMessagesModel(0);
    QVERIFY(msgModel != nullptr);
    QCOMPARE(msgModel->rowCount(), 2);
    QCOMPARE(logVm->data(logVm->index(0, 0), LogTaskModel::MessageCountRole).toInt(), 2);

    LogManager::instance().failTask(task->taskId(), "Resource compiler aborted");
    QTRY_COMPARE(logVm->data(logVm->index(0, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("FAILED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testTaskCancellation()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    auto task = LogManager::instance().createTask("Decompile BSP");
    task->start();
    task->info("Decompiling entities...");
    LogManager::instance().flushTask(task->taskId());

    QTRY_COMPARE(logVm->taskCount(), 1);
    QModelIndex taskIdx = logVm->index(0, 0);
    QCOMPARE(logVm->data(taskIdx, LogTaskModel::StateStringRole).toString(), QStringLiteral("RUNNING"));

    LogManager::instance().cancelTask(task->taskId(), "User clicked Cancel");
    QTRY_COMPARE(logVm->data(logVm->index(0, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("CANCELLED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testConcurrentTasksIsolation()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    const int threadCount = 4;
    const int logsPerThread = 25;
    std::vector<std::thread> workers;

    for (int i = 0; i < threadCount; ++i) {
        workers.emplace_back([i, logsPerThread]() {
            auto task = LogManager::instance().createTask(QStringLiteral("Worker Task %1").arg(i));
            task->start();
            for (int j = 0; j < logsPerThread; ++j) {
                task->info(QStringLiteral("Thread %1 emitting log #%2").arg(i).arg(j));
            }
            LogManager::instance().finishTask(task->taskId(), "Done");
        });
    }

    for (auto& t : workers) {
        t.join();
    }

    QTRY_COMPARE(logVm->taskCount(), threadCount);

    for (int i = 0; i < threadCount; ++i) {
        auto* msgModel = logVm->getTaskMessagesModel(i);
        QVERIFY(msgModel != nullptr);
        QVERIFY(msgModel->rowCount() >= logsPerThread);
    }

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testMultiBlockTask()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    auto task = LogManager::instance().createTask("Multi Block Test Task");
    task->start();

    const int messageCount = 150;
    for (int i = 0; i < messageCount; ++i) {
        task->debug(QStringLiteral("Log event %1").arg(i));
    }
    LogManager::instance().finishTask(task->taskId(), "Multi-block test done");

    QTRY_COMPARE(logVm->taskCount(), 1);
    auto* msgModel = logVm->getTaskMessagesModel(0);
    QVERIFY(msgModel != nullptr);
    QVERIFY(msgModel->rowCount() >= messageCount);

    for (int i = 0; i < messageCount; ++i) {
        QModelIndex idx = msgModel->index(i, 0);
        QCOMPARE(msgModel->data(idx, LogMessageListModel::MessageRole).toString(), QStringLiteral("Log event %1").arg(i));
    }

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testAsyncTaskRunnerNormal()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    bool callbackInvoked = false;
    int returnedResult = 0;

    AsyncTaskRunner::runTask<int>(
        QStringLiteral("Runner Task"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->info("Runner working...");
            }
            return TaskResult<int>::success(42);
        },
        [&](const TaskResult<int>& res) {
            callbackInvoked = true;
            if (res.isSuccess()) {
                returnedResult = res.value();
            }
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
    bool resultFailed = false;

    AsyncTaskRunner::runTask<int>(
        QStringLiteral("Faulty Runner Task"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->info("About to throw");
            }
            throw std::runtime_error("Simulated I/O failure");
        },
        [&](const TaskResult<int>& res) {
            callbackInvoked = true;
            resultFailed = res.isFailure();
        });

    QTRY_VERIFY(callbackInvoked);
    QVERIFY(resultFailed);

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

    AsyncTaskRunner::runTask<int>(
        QStringLiteral("Destroyed Context Task"),
        tempContext,
        [&workerRan](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->info("Worker executing while context is deleted");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            workerRan.store(true);
            return TaskResult<int>::success(100);
        },
        [](const TaskResult<int>&) {
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
    logVm.registerWithLogManager();

    auto task = LogManager::instance().createTask("Validate Source 2");
    task->start();
    task->info("Checking gameinfo.gi");
    task->warning("Optional addon missing");
    LogManager::instance().finishTask(task->taskId(), "Validation done");

    QTRY_COMPARE(logVm.taskCount(), 1);

    QString fullText = logVm.getFullLogText();
    QVERIFY(fullText.contains(QStringLiteral("=== Validate Source 2 ===")));
    QVERIFY(fullText.contains(QStringLiteral("Checking gameinfo.gi")));
    QVERIFY(fullText.contains(QStringLiteral("Optional addon missing")));
    QVERIFY(fullText.contains(QStringLiteral("INFO ")));
    QVERIFY(fullText.contains(QStringLiteral("WARN ")));
}

void TestAsyncTaskLogging::testExpandCollapseState()
{
    LogViewModel logVm;
    logVm.registerWithLogManager();

    auto task = LogManager::instance().createTask("Test Expand State");
    task->start();
    task->info("Step 1");
    LogManager::instance().finishTask(task->taskId(), "Done");

    QTRY_COMPARE(logVm.taskCount(), 1);

    QModelIndex idx = logVm.index(0, 0);
    QCOMPARE(logVm.data(idx, LogTaskModel::ExpandedRole).toBool(), true);

    logVm.toggleTaskExpanded(0);
    QCOMPARE(logVm.data(idx, LogTaskModel::ExpandedRole).toBool(), false);

    logVm.toggleTaskExpanded(0);
    QCOMPARE(logVm.data(idx, LogTaskModel::ExpandedRole).toBool(), true);
}

void TestAsyncTaskLogging::testHierarchicalSubModelGranularity()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    // 1. Create Root Task
    auto rootTask = LogManager::instance().createTask("Pipeline Root");
    rootTask->start();
    rootTask->info("Starting root pipeline step");
    LogManager::instance().flushTask(rootTask->taskId());

    QTRY_COMPARE(logVm->taskCount(), 1);
    QModelIndex rootIdx = logVm->index(0, 0);
    QCOMPARE(logVm->data(rootIdx, LogTaskModel::HasSubTasksRole).toBool(), false);
    QCOMPARE(logVm->data(rootIdx, LogTaskModel::SubTasksCountRole).toInt(), 0);

    // 2. Create Child Task 1
    auto child1 = LogManager::instance().createTask("Sub-step Extract VPK", rootTask->taskId());
    child1->start();
    child1->info("Extracting pak01_dir.vpk");
    LogManager::instance().flushTask(child1->taskId());

    // Root should now show hasSubTasks = true, subTasksCount = 1
    QTRY_COMPARE(logVm->data(rootIdx, LogTaskModel::HasSubTasksRole).toBool(), true);
    QCOMPARE(logVm->data(rootIdx, LogTaskModel::SubTasksCountRole).toInt(), 1);

    // 3. Create Child Task 2
    auto child2 = LogManager::instance().createTask("Sub-step Compile VMAT", rootTask->taskId());
    child2->start();
    child2->info("Compiling materials");
    LogManager::instance().flushTask(child2->taskId());

    QTRY_COMPARE(logVm->data(rootIdx, LogTaskModel::SubTasksCountRole).toInt(), 2);

    // Retrieve subTasksModel for rootTask
    auto* subTasksModel = logVm->getTaskSubTasksModel(0);
    QVERIFY(subTasksModel != nullptr);
    QCOMPARE(subTasksModel->taskCount(), 2);

    QModelIndex c1Idx = subTasksModel->index(0, 0);
    QCOMPARE(subTasksModel->data(c1Idx, LogTaskModel::TaskNameRole).toString(), QStringLiteral("Sub-step Extract VPK"));
    QCOMPARE(subTasksModel->data(c1Idx, LogTaskModel::DepthRole).toInt(), 1);
    QCOMPARE(subTasksModel->data(c1Idx, LogTaskModel::ParentTaskIdRole).toULongLong(), rootTask->taskId());

    QModelIndex c2Idx = subTasksModel->index(1, 0);
    QCOMPARE(subTasksModel->data(c2Idx, LogTaskModel::TaskNameRole).toString(), QStringLiteral("Sub-step Compile VMAT"));
    QCOMPARE(subTasksModel->data(c2Idx, LogTaskModel::DepthRole).toInt(), 1);
    QCOMPARE(subTasksModel->data(c2Idx, LogTaskModel::ParentTaskIdRole).toULongLong(), rootTask->taskId());

    // 4. Finish child 1 and child 2
    LogManager::instance().finishTask(child1->taskId(), "Extracted 10 files");
    LogManager::instance().finishTask(child2->taskId(), "Compiled 5 materials");
    LogManager::instance().finishTask(rootTask->taskId(), "Pipeline complete");

    QTRY_COMPARE(subTasksModel->data(c1Idx, LogTaskModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));
    QTRY_COMPARE(subTasksModel->data(c2Idx, LogTaskModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));
    QTRY_COMPARE(logVm->data(rootIdx, LogTaskModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testMultiLevelNestedTasksAndParallelChildExecution()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    // 1. Create Root Parent Task
    auto rootTask = LogManager::instance().createTask("Root Map Import");
    rootTask->start();
    rootTask->info("Starting multi-threaded parallel subtasks");
    LogManager::instance().flushTask(rootTask->taskId());

    QTRY_COMPARE(logVm->taskCount(), 1);

    // 2. Launch 3 parallel child tasks attached to rootTask->taskId()
    std::atomic<int> completedChildren{0};
    const int childCount = 3;

    for (int i = 0; i < childCount; ++i) {
        AsyncTaskRunner::runChildTask<int>(
            rootTask->taskId(),
            QStringLiteral("Parallel Child Task %1").arg(i + 1),
            this,
            [i](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
                if (ctx) {
                    ctx->info(QStringLiteral("Child %1 working in parallel").arg(i + 1));
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    ctx->info(QStringLiteral("Child %1 finished work").arg(i + 1));
                }
                return TaskResult<int>::success(i + 1);
            },
            [&completedChildren](const TaskResult<int>&) {
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
        QTRY_COMPARE(subTasksModel->data(childIdx, LogTaskModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));
        QVERIFY(subTasksModel->data(childIdx, LogTaskModel::MessageCountRole).toInt() >= 2);
    }

    LogManager::instance().finishTask(rootTask->taskId(), "Root pipeline done");

    // Test recursive formatted log text
    QTRY_VERIFY(logVm->getFullLogText().contains(QStringLiteral("=== Root Map Import ===")));
    QVERIFY(logVm->getFullLogText().contains(QStringLiteral("--- Parallel Child Task 1 ---")));
    QVERIFY(logVm->getFullLogText().contains(QStringLiteral("--- Parallel Child Task 2 ---")));
    QVERIFY(logVm->getFullLogText().contains(QStringLiteral("--- Parallel Child Task 3 ---")));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testNullContextAndEmptyCallbackExecution()
{
    std::atomic<bool> workerRan{false};

    // 1. Run without context or callback (fire-and-forget background task)
    AsyncTaskRunner::runBackground(
        QStringLiteral("Headless Background Task"),
        [&workerRan](std::shared_ptr<TaskLoggingContext> ctx) {
            if (ctx) {
                ctx->info("Headless task working in background");
            }
            workerRan.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(workerRan.load(), 3000);

    // Verify task completed cleanly in LogManager
    QTRY_VERIFY_WITH_TIMEOUT(LogManager::instance().taskCount() >= 1, 3000);

    // 2. Run with context but omitted callback
    std::atomic<int> computeResult{0};
    AsyncTaskRunner::runTask<int>(
        QStringLiteral("Void Callback Task"),
        this,
        [&computeResult](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->info("Calculating value");
            }
            computeResult.store(42);
            return TaskResult<int>::success(42);
        });

    QTRY_COMPARE_WITH_TIMEOUT(computeResult.load(), 42, 3000);
    QThreadPool::globalInstance()->waitForDone(3000);
}

void TestAsyncTaskLogging::testSemanticBusinessFailureDetection()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    // 1. Worker returning TaskResult::failure and logging error (no exception thrown)
    std::atomic<bool> callbackFired{false};
    AsyncTaskRunner::runTask<QString>(
        QStringLiteral("Validate Source 1 Failure Test"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<QString> {
            if (ctx) {
                ctx->info("Starting validation");
                ctx->error("gameinfo.txt not found");
            }
            return TaskResult<QString>::failure(QStringLiteral("gameinfo.txt not found"));
        },
        [&callbackFired](const TaskResult<QString>& res) {
            QVERIFY(res.isFailure());
            callbackFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(callbackFired.load(), 3000);
    QTRY_COMPARE(logVm->taskCount(), 1);
    QTRY_COMPARE(logVm->data(logVm->index(0, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("FAILED"));

    // 2. Worker returning TaskResult<void>::failure
    std::atomic<bool> failureCallbackFired{false};
    AsyncTaskRunner::runTask<void>(
        QStringLiteral("Void Failure Test"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<void> {
            if (ctx) {
                ctx->info("Checking condition");
            }
            return TaskResult<void>::failure(QStringLiteral("Condition failed"));
        },
        [&failureCallbackFired](const TaskResult<void>& res) {
            QVERIFY(res.isFailure());
            failureCallbackFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(failureCallbackFired.load(), 3000);
    QTRY_COMPARE(logVm->taskCount(), 2);
    QTRY_COMPARE(logVm->data(logVm->index(1, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("FAILED"));

    // 3. Worker returning valid result with NO errors -> COMPLETED
    std::atomic<bool> successCallbackFired{false};
    AsyncTaskRunner::runTask<QString>(
        QStringLiteral("Successful Validation Test"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<QString> {
            if (ctx) {
                ctx->info("Starting validation");
                ctx->info("Validation succeeded");
            }
            return TaskResult<QString>::success(QStringLiteral("Valid Installation"));
        },
        [&successCallbackFired](const TaskResult<QString>& res) {
            QVERIFY(res.isSuccess());
            successCallbackFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(successCallbackFired.load(), 3000);
    QTRY_COMPARE(logVm->taskCount(), 3);
    QTRY_COMPARE(logVm->data(logVm->index(2, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testTaskResultOutcomes()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    // 1. TaskResult::success
    std::atomic<bool> successFired{false};
    AsyncTaskRunner::runTask<int>(
        QStringLiteral("TaskResult Success"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->info("Step 1 done");
            }
            return TaskResult<int>::success(100);
        },
        [&successFired](const TaskResult<int>& res) {
            QVERIFY(res.isSuccess());
            QCOMPARE(res.value(), 100);
            successFired.store(true);
        });
    QTRY_VERIFY_WITH_TIMEOUT(successFired.load(), 3000);
    QTRY_COMPARE(logVm->taskCount(), 1);
    QTRY_COMPARE(logVm->data(logVm->index(0, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));

    // 2. TaskResult::failure with partial value
    std::atomic<bool> failureFired{false};
    AsyncTaskRunner::runTask<int>(
        QStringLiteral("TaskResult Failure"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->info("Step failed");
            }
            return TaskResult<int>::failure(QStringLiteral("Asset not found"), 50);
        },
        [&failureFired](const TaskResult<int>& res) {
            QVERIFY(res.isFailure());
            QCOMPARE(res.message(), QStringLiteral("Asset not found"));
            QVERIFY(res.hasValue());
            QCOMPARE(res.value(), 50);
            failureFired.store(true);
        });
    QTRY_VERIFY_WITH_TIMEOUT(failureFired.load(), 3000);
    QTRY_COMPARE(logVm->taskCount(), 2);
    QTRY_COMPARE(logVm->data(logVm->index(1, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("FAILED"));

    // 3. TaskResult::cancelled
    std::atomic<bool> cancelFired{false};
    AsyncTaskRunner::runTask<QString>(
        QStringLiteral("TaskResult Cancelled"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<QString> {
            if (ctx) {
                ctx->info("User requested cancellation");
            }
            return TaskResult<QString>::cancelled(QStringLiteral("User aborted import"));
        },
        [&cancelFired](const TaskResult<QString>& res) {
            QVERIFY(res.isCancelled());
            cancelFired.store(true);
        });
    QTRY_VERIFY_WITH_TIMEOUT(cancelFired.load(), 3000);
    QTRY_COMPARE(logVm->taskCount(), 3);
    QTRY_COMPARE(logVm->data(logVm->index(2, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("CANCELLED"));

    // 4. TaskResult::skipped
    std::atomic<bool> skipFired{false};
    AsyncTaskRunner::runTask<void>(
        QStringLiteral("TaskResult Skipped"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<void> {
            if (ctx) {
                ctx->info("Asset already up to date, skipping");
            }
            return TaskResult<void>::skipped(QStringLiteral("Already compiled"));
        },
        [&skipFired](const TaskResult<void>& res) {
            QVERIFY(res.isSkipped());
            skipFired.store(true);
        });
    QTRY_VERIFY_WITH_TIMEOUT(skipFired.load(), 3000);
    QTRY_COMPARE(logVm->taskCount(), 4);
    QTRY_COMPARE(logVm->data(logVm->index(3, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("SKIPPED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testInvalidParentTaskRejection()
{
    // 1. Calling runChildTask with an invalid non-existent parentTaskId (e.g. 999999)
    std::atomic<bool> workerRan{false};
    std::atomic<bool> callbackFired{false};
    TaskResult<int> receivedResult;

    AsyncTaskRunner::runChildTask<int>(
        999999, // non-existent parent ID
        QStringLiteral("Orphaned Child Task"),
        this,
        [&workerRan](std::shared_ptr<TaskLoggingContext>) -> TaskResult<int> {
            workerRan.store(true);
            return TaskResult<int>::success(42);
        },
        [&callbackFired, &receivedResult](const TaskResult<int>& res) {
            receivedResult = res;
            callbackFired.store(true);
        });

    // Wait a brief period and process events
    QTRY_VERIFY_WITH_TIMEOUT(callbackFired.load(), 2000);

    // Assert that worker was STRICTLY NEVER executed!
    QCOMPARE(workerRan.load(), false);
    QVERIFY(receivedResult.isFailure());
    QVERIFY(receivedResult.message().contains(QStringLiteral("invalid parentTaskId")));

    // 2. Calling runChildTask<void> with an invalid non-existent parentTaskId
    std::atomic<bool> voidWorkerRan{false};
    std::atomic<bool> voidCallbackFired{false};
    TaskResult<void> voidResult;

    AsyncTaskRunner::runChildTask<void>(
        888888, // non-existent parent ID
        QStringLiteral("Orphaned Void Task"),
        this,
        [&voidWorkerRan](std::shared_ptr<TaskLoggingContext>) -> TaskResult<void> {
            voidWorkerRan.store(true);
            return TaskResult<void>::success();
        },
        [&voidCallbackFired, &voidResult](const TaskResult<void>& res) {
            voidResult = res;
            voidCallbackFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(voidCallbackFired.load(), 2000);
    QCOMPARE(voidWorkerRan.load(), false);
    QVERIFY(voidResult.isFailure());
}

void TestAsyncTaskLogging::testLoggedErrorForcesTaskFailure()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    std::atomic<bool> callbackFired{false};
    AsyncTaskRunner::runTask<int>(
        QStringLiteral("Success With Logged Error Task"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->info("Beginning work...");
                ctx->error("Unrecoverable sub-step failed!");
            }
            // Even though worker returns success, logged error must force task state to FAILED
            return TaskResult<int>::success(42);
        },
        [&callbackFired](const TaskResult<int>& res) {
            QVERIFY(res.isFailure());
            QCOMPARE(res.value(), 42); // Partial value preserved
            callbackFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(callbackFired.load(), 3000);
    QTRY_COMPARE(logVm->taskCount(), 1);
    QTRY_COMPARE(logVm->data(logVm->index(0, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("FAILED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testLoggedWarningPreservesTaskCompleted()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    std::atomic<bool> callbackFired{false};
    AsyncTaskRunner::runTask<int>(
        QStringLiteral("Success With Warning Task"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->info("Beginning work...");
                ctx->warning("Recoverable warning occurred, continuing with fallback.");
            }
            return TaskResult<int>::success(42);
        },
        [&callbackFired](const TaskResult<int>& res) {
            QVERIFY(res.isSuccess());
            callbackFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(callbackFired.load(), 3000);
    QTRY_COMPARE(logVm->taskCount(), 1);
    QTRY_COMPARE(logVm->data(logVm->index(0, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testRunTaskAndChildTaskApi()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    // 1. runTask<int>
    std::atomic<bool> runTaskFired{false};
    AsyncTaskRunner::runTask<int>(
        QStringLiteral("RunTask Integer Test"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->info("Computing value in runTask...");
            }
            return TaskResult<int>::success(12345);
        },
        [&runTaskFired](const TaskResult<int>& res) {
            QVERIFY(res.isSuccess());
            QCOMPARE(res.value(), 12345);
            runTaskFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(runTaskFired.load(), 3000);
    QTRY_COMPARE(logVm->taskCount(), 1);
    QTRY_COMPARE(logVm->data(logVm->index(0, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));

    // 2. runTask<void>
    std::atomic<bool> voidTaskFired{false};
    AsyncTaskRunner::runTask<void>(
        QStringLiteral("RunTask Void Test"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<void> {
            if (ctx) {
                ctx->info("Executing void operation...");
            }
            return TaskResult<void>::success(QStringLiteral("Void operation succeeded"));
        },
        [&voidTaskFired](const TaskResult<void>& res) {
            QVERIFY(res.isSuccess());
            voidTaskFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(voidTaskFired.load(), 3000);
    QTRY_COMPARE(logVm->taskCount(), 2);
    QTRY_COMPARE(logVm->data(logVm->index(1, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("COMPLETED"));

    // 3. runChildTask<QString>
    auto rootContext = LogManager::instance().createTask("Parent Task for Child");
    rootContext->start();
    std::atomic<bool> childTaskFired{false};

    AsyncTaskRunner::runChildTask<QString>(
        rootContext->taskId(),
        QStringLiteral("Child Task Via runChildTask"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<QString> {
            if (ctx) {
                ctx->info("Child task executing...");
            }
            return TaskResult<QString>::success(QStringLiteral("Child Result Data"));
        },
        [&childTaskFired](const TaskResult<QString>& res) {
            QVERIFY(res.isSuccess());
            QCOMPARE(res.value(), QStringLiteral("Child Result Data"));
            childTaskFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(childTaskFired.load(), 3000);
    LogManager::instance().finishTask(rootContext->taskId(), "Parent done");

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testExplicitFailWithTaskResultSuccessContradiction()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    std::atomic<bool> callbackFired{false};
    TaskResult<int> receivedResult = TaskResult<int>::success(0);

    AsyncTaskRunner::runTask<int>(
        QStringLiteral("Contradiction Fail + Success"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->fail("Worker explicit failure");
            }
            return TaskResult<int>::success(42);
        },
        [&callbackFired, &receivedResult](const TaskResult<int>& res) {
            receivedResult = res;
            callbackFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(callbackFired.load(), 3000);

    // Business outcome MUST be converted to failure
    QVERIFY(receivedResult.isFailure());
    QCOMPARE(receivedResult.value(), 42); // Partial value preserved

    // UI Log plane MUST show FAILED
    QCOMPARE(logVm->taskCount(), 1);
    QCOMPARE(logVm->data(logVm->index(0, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("FAILED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testExplicitCancelWithTaskResultSuccessContradiction()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    std::atomic<bool> callbackFired{false};
    TaskResult<int> receivedResult = TaskResult<int>::success(0);

    AsyncTaskRunner::runTask<int>(
        QStringLiteral("Contradiction Cancel + Success"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->cancel("Worker explicit cancel");
            }
            return TaskResult<int>::success(42);
        },
        [&callbackFired, &receivedResult](const TaskResult<int>& res) {
            receivedResult = res;
            callbackFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(callbackFired.load(), 3000);

    // Business outcome MUST be converted to cancelled
    QVERIFY(receivedResult.isCancelled());
    QCOMPARE(receivedResult.value(), 42);

    // UI Log plane MUST show CANCELLED
    QCOMPARE(logVm->taskCount(), 1);
    QCOMPARE(logVm->data(logVm->index(0, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("CANCELLED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testExplicitSkipWithTaskResultSuccessContradiction()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    std::atomic<bool> callbackFired{false};
    TaskResult<int> receivedResult = TaskResult<int>::success(0);

    AsyncTaskRunner::runTask<int>(
        QStringLiteral("Contradiction Skip + Success"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->skip("Worker explicit skip");
            }
            return TaskResult<int>::success(42);
        },
        [&callbackFired, &receivedResult](const TaskResult<int>& res) {
            receivedResult = res;
            callbackFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(callbackFired.load(), 3000);

    // Business outcome MUST be converted to skipped
    QVERIFY(receivedResult.isSkipped());
    QCOMPARE(receivedResult.value(), 42);

    // UI Log plane MUST show SKIPPED
    QCOMPARE(logVm->taskCount(), 1);
    QCOMPARE(logVm->data(logVm->index(0, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("SKIPPED"));

    logVm->unregisterFromLogManager();
}

void TestAsyncTaskLogging::testExplicitCompleteWithTaskResultFailureContradiction()
{
    auto logVm = std::make_shared<LogViewModel>();
    logVm->registerWithLogManager();

    std::atomic<bool> callbackFired{false};
    TaskResult<int> receivedResult = TaskResult<int>::success(0);

    AsyncTaskRunner::runTask<int>(
        QStringLiteral("Contradiction Complete + Failure"),
        this,
        [](std::shared_ptr<TaskLoggingContext> ctx) -> TaskResult<int> {
            if (ctx) {
                ctx->complete("Worker completed early");
            }
            return TaskResult<int>::failure("Fatal backend error", 10);
        },
        [&callbackFired, &receivedResult](const TaskResult<int>& res) {
            receivedResult = res;
            callbackFired.store(true);
        });

    QTRY_VERIFY_WITH_TIMEOUT(callbackFired.load(), 3000);

    // Business outcome remains failure
    QVERIFY(receivedResult.isFailure());
    QCOMPARE(receivedResult.value(), 10);

    // UI Log plane MUST be forced to FAILED
    QCOMPARE(logVm->taskCount(), 1);
    QCOMPARE(logVm->data(logVm->index(0, 0), LogTaskModel::StateStringRole).toString(), QStringLiteral("FAILED"));

    logVm->unregisterFromLogManager();
}

QTEST_MAIN(TestAsyncTaskLogging)
#include "TestAsyncTaskLogging.moc"
