#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QRunnable>
#include <QPointer>
#include <QMetaObject>
#include <memory>
#include <functional>
#include <utility>
#include <type_traits>

#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Logging/TaskState.h"
#include "Core/Result/Result.h"
#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Error/Exception.h"
#include "Application/Execution/ExecutionGuard.h"
#include "Application/Async/SystemTaskLog.h"
#include "Application/Async/TaskHandle.h"

namespace Application::Async {

using Core::Result;
using Core::ResultStatus;

namespace Detail {

template <typename Fn>
bool isCallableValid(const Fn& fn) {
    if constexpr (std::is_convertible_v<Fn, bool>) {
        return !!fn;
    } else {
        return true;
    }
}

} // namespace Detail

/**
 * @brief Dispatcher and coordinator for asynchronous tasks, bridging Task Execution Lifecycle
 * (Core::Logging::TaskState) and Business Outcome (Core::Result<T>).
 *
 * Architectural Dual-Plane Model:
 * 1. **Execution Lifecycle Plane (`TaskState`)**:
 *    - Managed by `LogManager` / `TaskLoggingContext` (`Pending` -> `Running` -> `Completed` | `Failed` | `Cancelled` | `Skipped`).
 *    - Tracked and rendered in UI log models (`LogViewModel`, `LogTaskModel`).
 * 2. **Business Outcome Plane (`Result<T>`)**:
 *    - Standard single-layer return contract for Workflow and Application APIs (`Success`, `Failure`, `Cancelled`, `Skipped` + payload `T`).
 *
 * Public Business API:
 * - `runTask<T>(taskName, context, worker, callback)`: For tasks with business payload `T`.
 * - `runTask<void>(taskName, context, worker, callback)`: For tasks without payload, retaining full `Result<void>` outcome semantics.
 * - `runChildTask<T>(parentTaskId, taskName, context, worker, callback)`: For hierarchical child sub-tasks.
 * - `runChildTask<void>(parentTaskId, taskName, context, worker, callback)`: For void child sub-tasks.
 * - `runBackground(taskName, worker)`: For fire-and-forget background logging tasks.
 */
class AsyncTaskRunner {
public:
    /**
     * @brief Primary API: Runs an async task whose business outcome is Result<T>.
     *
     * @tparam T The business payload type (e.g. GameInstallationInfo, DetectionResult, or void).
     * @param taskName The name of the task for logging/UI display.
     * @param context The Qt lifetime context object (callback marshaled to its thread).
     * @param worker Lambda taking std::shared_ptr<TaskLoggingContext> and returning Result<T>.
     * @param callback Callback receiving const Result<T>&.
     * @param pool The QThreadPool to dispatch to (defaults to globalInstance).
     * @param parentTaskId Optional parent task ID for hierarchical sub-tasks.
     */
    template <typename T = void, typename WorkerFn, typename CallbackFn = std::function<void(const Result<T>&)>>
    static TaskHandle runTask(
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback = CallbackFn{},
        QThreadPool* pool = QThreadPool::globalInstance(),
        quint64 parentTaskId = 0)
    {
        return runTaskInternal<T>(taskName, context, std::forward<WorkerFn>(worker), std::forward<CallbackFn>(callback), pool, parentTaskId);
    }

    /**
     * @brief Runs an async workflow task with a dedicated log directory.
     *
     * @tparam T The business payload type.
     * @param workflowName The name of the workflow task.
     * @param assetBaseName The primary asset base name (used for tool log naming).
     * @param context The Qt lifetime context object.
     * @param worker Lambda taking std::shared_ptr<TaskLoggingContext> and returning Result<T>.
     * @param callback Callback receiving const Result<T>&.
     * @param pool The QThreadPool to dispatch to.
     */
    template <typename T = void, typename WorkerFn, typename CallbackFn = std::function<void(const Result<T>&)>>
    static TaskHandle runWorkflowTask(
        const QString& workflowName,
        const QString& assetBaseName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback = CallbackFn{},
        QThreadPool* pool = QThreadPool::globalInstance())
    {
        return runTaskInternal<T>(workflowName, context, std::forward<WorkerFn>(worker), std::forward<CallbackFn>(callback), pool, 0, true, assetBaseName);
    }

    /**
     * @brief Primary API: Runs an async child sub-task whose business outcome is Result<T>.
     *
     * @tparam T The business payload type (e.g. GameInstallationInfo, DetectionResult, or void).
     * @param parentTaskId Parent task ID in LogManager.
     * @param taskName The name of the child sub-task.
     * @param context The Qt lifetime context object.
     * @param worker Lambda taking std::shared_ptr<TaskLoggingContext> (and optional CancellationToken) and returning Result<T>.
     * @param callback Callback receiving const Result<T>&.
     * @param pool The QThreadPool to dispatch to.
     */
    template <typename T = void, typename WorkerFn, typename CallbackFn = std::function<void(const Result<T>&)>>
    static TaskHandle runChildTask(
        quint64 parentTaskId,
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback = CallbackFn{},
        QThreadPool* pool = QThreadPool::globalInstance())
    {
        return runTaskInternal<T>(taskName, context, std::forward<WorkerFn>(worker), std::forward<CallbackFn>(callback), pool, parentTaskId);
    }

    /**
     * @brief Runs a fire-and-forget background worker task returning Result<void> without UI callback.
     *
     * Convenience wrapper delegating to runTask<void> to ensure unified execution lifecycle,
     * structured error handling, and state arbitration across all tasks.
     * Worker is strictly constrained to return Core::Result<void>.
     */
    template <typename WorkerFn>
    static TaskHandle runBackground(
        const QString& taskName,
        WorkerFn&& worker,
        QThreadPool* pool = QThreadPool::globalInstance(),
        quint64 parentTaskId = 0)
    {
        using DecayedWorker = std::decay_t<WorkerFn>;
        static_assert(
            std::is_invocable_r_v<Result<void>, DecayedWorker, std::shared_ptr<Core::Logging::TaskLoggingContext>> ||
            std::is_invocable_r_v<Result<void>, DecayedWorker, std::shared_ptr<Core::Logging::TaskLoggingContext>, Core::Async::CancellationToken>,
            "AsyncTaskRunner::runBackground worker must return Core::Result<void>");
        return runTask<void>(taskName, nullptr, std::forward<WorkerFn>(worker), {}, pool, parentTaskId);
    }

    template <typename WorkerFn>
    static TaskHandle runTask(
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        QThreadPool* pool = QThreadPool::globalInstance(),
        quint64 parentTaskId = 0)
    {
        return runTaskInternal<void>(taskName, context, std::forward<WorkerFn>(worker), std::function<void(const Result<void>&)>{}, pool, parentTaskId);
    }

    /**
     * @brief Runs a system background task that is deliberately invisible to the
     * workflow log plane: no LogManager task, no TaskState, no per-task log file,
     * and no UI task tree entry. Worker entries are merged into the application
     * log under a "[TaskName]" prefix; the runner writes one lifecycle line
     * (started / finished / failed / cancelled / skipped) around the worker.
     *
     * The returned TaskHandle carries taskId 0, so cancel() cooperatively trips
     * only the cancellation token without touching LogManager.
     *
     * @tparam T The business payload type.
     * @param context The Qt lifetime context object (callback marshaled to its thread).
     * @param worker Lambda taking const SystemTaskLog& (and optionally the CancellationToken) and returning Result<T>.
     */
    template <typename T = void, typename WorkerFn, typename CallbackFn = std::function<void(const Result<T>&)>>
    static TaskHandle runSystemTask(
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback = CallbackFn{},
        QThreadPool* pool = QThreadPool::globalInstance())
    {
        using DecayedWorker = std::decay_t<WorkerFn>;
        using DecayedCallback = std::decay_t<CallbackFn>;
        static_assert(
            std::is_invocable_r_v<Result<T>, DecayedWorker, const SystemTaskLog&, Core::Async::CancellationToken> ||
            std::is_invocable_r_v<Result<T>, DecayedWorker, const SystemTaskLog&>,
            "AsyncTaskRunner::runSystemTask worker must return Core::Result<T> and take const SystemTaskLog& (and optionally the CancellationToken)");

        QPointer<QObject> contextGuard(context);
        Core::Async::CancellationToken token;
        TaskHandle handle(0, token);

        bool hasValidCallback = false;
        if constexpr (std::is_invocable_v<DecayedCallback, Result<T>>) {
            hasValidCallback = Detail::isCallableValid(callback);
        }

        auto workerLambda = [taskName, contextGuard, context, hasValidCallback, token,
                             worker = DecayedWorker(std::forward<WorkerFn>(worker)),
                             callback = DecayedCallback(std::forward<CallbackFn>(callback))]() mutable {
            const SystemTaskLog sysLog(taskName);
            sysLog.info(QStringLiteral("started"));

            Result<T> result{};
            try {
                if constexpr (std::is_invocable_v<DecayedWorker, const SystemTaskLog&, Core::Async::CancellationToken>) {
                    result = worker(sysLog, token);
                } else {
                    result = worker(sysLog);
                }
            } catch (const Core::Error::Exception& ex) {
                const QString detailInfo = ex.details().isEmpty()
                    ? (ex.message().isEmpty() ? QString::fromUtf8(ex.what()) : ex.message())
                    : QCoreApplication::translate("AsyncTaskRunner", "%1 (%2)").arg(ex.message().isEmpty() ? QString::fromUtf8(ex.what()) : ex.message(), ex.details());
                sysLog.error(QStringLiteral("Task exception [%1]: %2")
                    .arg(static_cast<int>(ex.errorCode()))
                    .arg(detailInfo));
                result = Execution::ExecutionGuard::handleException<T>(
                    ex, QCoreApplication::translate("AsyncTaskRunner", "Task '%1' failed").arg(taskName));
            } catch (const std::exception& ex) {
                sysLog.error(QStringLiteral("Unhandled standard exception: %1").arg(QString::fromUtf8(ex.what())));
                result = Execution::ExecutionGuard::handleException<T>(
                    ex, QCoreApplication::translate("AsyncTaskRunner", "Task '%1' failed").arg(taskName));
            } catch (...) {
                sysLog.error(QStringLiteral("Unhandled unknown exception in task"));
                result = Execution::ExecutionGuard::handleUnknownException<T>(
                    QCoreApplication::translate("AsyncTaskRunner", "Task '%1' failed").arg(taskName));
            }

            // Lifecycle outcome line (no TaskState plane; the application log is the record)
            if (result.isSuccess()) {
                sysLog.info(result.message().isEmpty()
                    ? QStringLiteral("finished")
                    : QStringLiteral("finished: %1").arg(result.message()));
            } else if (result.isCancelled()) {
                sysLog.warning(result.message().isEmpty()
                    ? QStringLiteral("cancelled")
                    : QStringLiteral("cancelled: %1").arg(result.message()));
            } else if (result.isSkipped()) {
                sysLog.info(result.message().isEmpty()
                    ? QStringLiteral("skipped")
                    : QStringLiteral("skipped: %1").arg(result.message()));
            } else {
                sysLog.error(result.message().isEmpty()
                    ? QStringLiteral("failed")
                    : QStringLiteral("failed: %1").arg(result.message()));
            }

            if constexpr (std::is_invocable_v<DecayedCallback, Result<T>>) {
                if (hasValidCallback) {
                    if (contextGuard) {
                        QMetaObject::invokeMethod(contextGuard.data(), [contextGuard, cb = std::move(callback), res = std::move(result)]() {
                            try {
                                if (contextGuard && Detail::isCallableValid(cb)) {
                                    cb(res);
                                }
                            } catch (...) {}
                        }, Qt::QueuedConnection);
                    } else if (!context) {
                        try {
                            callback(result);
                        } catch (...) {}
                    }
                }
            }
        };

        QRunnable* runnable = QRunnable::create(std::move(workerLambda));
        if (pool) {
            pool->start(runnable);
        } else {
            QThreadPool::globalInstance()->start(runnable);
        }
        return handle;
    }

private:
    /**
     * @brief Internal engine executing a typed Result<T> async worker.
     */
    template <typename T, typename WorkerFn, typename CallbackFn>
    static TaskHandle runTaskInternal(
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback,
        QThreadPool* pool,
        quint64 parentTaskId,
        bool isWorkflow = false,
        const QString& workflowAssetBaseName = QString())
    {
        using DecayedWorker = std::decay_t<WorkerFn>;
        using DecayedCallback = std::decay_t<CallbackFn>;

        auto taskContext = isWorkflow
            ? Core::Logging::LogManager::instance().createWorkflowTask(taskName, workflowAssetBaseName)
            : Core::Logging::LogManager::instance().createTask(taskName, parentTaskId);
        if (!taskContext) {
            // Task creation rejected (e.g. invalid parentTaskId). Do NOT dispatch worker.
            // Safely deliver explicit Result<T>::failure to callback.
            if constexpr (std::is_invocable_v<DecayedCallback, Result<T>>) {
                if (Detail::isCallableValid(callback)) {
                    Result<T> failureResult = Result<T>::failure(
                        Core::Error::ErrorCode::OperationFailed,
                        QCoreApplication::translate("AsyncTaskRunner", "Failed to create task context for '%1' (invalid parentTaskId: %2)")
                            .arg(taskName).arg(parentTaskId));
                    if (context) {
                        QPointer<QObject> guard(context);
                        QMetaObject::invokeMethod(guard.data(), [guard, cb = DecayedCallback(std::forward<CallbackFn>(callback)), res = std::move(failureResult)]() {
                            try {
                                if (guard && Detail::isCallableValid(cb)) {
                                    cb(res);
                                }
                            } catch (...) {}
                        }, Qt::QueuedConnection);
                    } else {
                        try {
                            callback(failureResult);
                        } catch (...) {}
                    }
                }
            }
            return TaskHandle{};
        }

        taskContext->start();
        QPointer<QObject> contextGuard(context);
        quint64 taskId = taskContext->taskId();
        Core::Async::CancellationToken token;
        TaskHandle handle(taskId, token);

        bool hasValidCallback = false;
        if constexpr (std::is_invocable_v<DecayedCallback, Result<T>>) {
            hasValidCallback = Detail::isCallableValid(callback);
        }

        auto workerLambda = [taskContext, taskId, taskName, contextGuard, context, hasValidCallback, token,
                             worker = DecayedWorker(std::forward<WorkerFn>(worker)),
                             callback = DecayedCallback(std::forward<CallbackFn>(callback))]() mutable {
            try {
                Result<T> result{};
                bool threwException = false;

                try {
                    if constexpr (std::is_invocable_v<DecayedWorker, std::shared_ptr<Core::Logging::TaskLoggingContext>, Core::Async::CancellationToken>) {
                        result = worker(taskContext, token);
                    } else {
                        result = worker(taskContext);
                    }
                } catch (const Core::Error::Exception& ex) {
                    threwException = true;
                    if (taskContext) {
                        const QString detailInfo = ex.details().isEmpty()
                            ? (ex.message().isEmpty() ? QString::fromUtf8(ex.what()) : ex.message())
                            : QCoreApplication::translate("AsyncTaskRunner", "%1 (%2)").arg(ex.message().isEmpty() ? QString::fromUtf8(ex.what()) : ex.message(), ex.details());
                        taskContext->error(QStringLiteral("Task exception [%1]: %2")
                            .arg(static_cast<int>(ex.errorCode()))
                            .arg(detailInfo));
                    }
                    result = Execution::ExecutionGuard::handleException<T>(
                        ex, QCoreApplication::translate("AsyncTaskRunner", "Task '%1' failed").arg(taskName));
                } catch (const std::exception& ex) {
                    threwException = true;
                    if (taskContext) {
                        taskContext->error(QStringLiteral("Unhandled standard exception: %1").arg(QString::fromUtf8(ex.what())));
                    }
                    result = Execution::ExecutionGuard::handleException<T>(
                        ex, QCoreApplication::translate("AsyncTaskRunner", "Task '%1' failed").arg(taskName));
                } catch (...) {
                    threwException = true;
                    if (taskContext) {
                        taskContext->error(QStringLiteral("Unhandled unknown exception in task"));
                    }
                    result = Execution::ExecutionGuard::handleUnknownException<T>(
                        QCoreApplication::translate("AsyncTaskRunner", "Task '%1' failed").arg(taskName));
                }

                if (taskContext) {
                    if (threwException) {
                        QString taskSummary = QCoreApplication::translate("AsyncTaskRunner", "Task failed with uncaught exception");
                        Core::Logging::LogManager::instance().forceTaskState(
                            taskId, Core::Logging::TaskState::Failed, taskSummary);
                    } else {
                        const auto currentState = taskContext->state();
                        const bool hasErrors = taskContext->hasErrors();

                        auto makeContractFailure = [&](const QString& msg) {
                            // Preserve original business error when available;
                            // contract violation is already logged via taskContext->error().
                            auto error = result.error().isSuccess()
                                ? Core::Error::Error(Core::Error::ErrorCode::OperationFailed, msg)
                                : result.error();

                            if constexpr (std::is_void_v<T>) {
                                return Result<T>::failure(std::move(error), msg);
                            } else {
                                return Result<T>::failure(
                                    std::move(error),
                                    msg,
                                    result.hasValue() ? std::make_optional(result.value()) : std::nullopt);
                            }
                        };

                        auto makeContractCancelled = [&](const QString& msg) {
                            if constexpr (std::is_void_v<T>) {
                                return Result<T>::cancelled(msg);
                            } else {
                                return Result<T>::cancelled(
                                    msg,
                                    result.hasValue() ? std::make_optional(result.value()) : std::nullopt);
                            }
                        };

                        auto makeContractSkipped = [&](const QString& msg) {
                            if constexpr (std::is_void_v<T>) {
                                return Result<T>::skipped(msg);
                            } else {
                                return Result<T>::skipped(
                                    msg,
                                    result.hasValue() ? std::make_optional(result.value()) : std::nullopt);
                            }
                        };

                        // Cross-terminal state conflict arbitration matrix
                        if (currentState == Core::Logging::TaskState::Failed || hasErrors) {
                            // Priority 1: Failed / Logged errors dominate
                            if (result.isSuccess()) {
                                if (!hasErrors) {
                                    taskContext->error(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: worker returned Result::success after task failed"));
                                }
                                result = makeContractFailure(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: Task completed with logged errors or explicit failure"));
                            } else if (result.isCancelled()) {
                                taskContext->error(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: worker returned Result::cancelled after task failed with errors"));
                                result = makeContractFailure(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: Task failed with errors before cancellation"));
                            } else if (result.isSkipped()) {
                                taskContext->error(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: worker returned Result::skipped after task failed with errors"));
                                result = makeContractFailure(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: Task failed with errors before skipping"));
                            }
                            Core::Logging::LogManager::instance().forceTaskState(
                                taskId, Core::Logging::TaskState::Failed,
                                result.message().isEmpty() ? QCoreApplication::translate("AsyncTaskRunner", "Task failed") : result.message());

                        } else if (currentState == Core::Logging::TaskState::Cancelled) {
                            // Priority 2: Cancelled (without errors)
                            if (result.isSuccess()) {
                                taskContext->warning(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: worker returned Result::success after task was cancelled"));
                                result = makeContractCancelled(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: Task was cancelled"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Cancelled, QCoreApplication::translate("AsyncTaskRunner", "Cancelled"));
                            } else if (result.isFailure()) {
                                taskContext->warning(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: worker returned Result::failure after task was cancelled"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Failed,
                                    result.message().isEmpty() ? QCoreApplication::translate("AsyncTaskRunner", "Task failed") : result.message());
                            } else if (result.isSkipped()) {
                                taskContext->warning(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: worker returned Result::skipped after task was cancelled"));
                                result = makeContractCancelled(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: Task was cancelled"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Cancelled, QCoreApplication::translate("AsyncTaskRunner", "Cancelled"));
                            } else { // result.isCancelled() -> Agreement
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Cancelled,
                                    result.message().isEmpty() ? QCoreApplication::translate("AsyncTaskRunner", "Cancelled") : result.message());
                            }

                        } else if (currentState == Core::Logging::TaskState::Skipped) {
                            // Priority 3: Skipped (without errors)
                            if (result.isSuccess()) {
                                taskContext->warning(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: worker returned Result::success after task was skipped"));
                                result = makeContractSkipped(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: Task was skipped"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Skipped, QCoreApplication::translate("AsyncTaskRunner", "Skipped"));
                            } else if (result.isFailure()) {
                                taskContext->warning(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: worker returned Result::failure after task was skipped"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Failed,
                                    result.message().isEmpty() ? QCoreApplication::translate("AsyncTaskRunner", "Task failed") : result.message());
                            } else if (result.isCancelled()) {
                                taskContext->warning(QCoreApplication::translate("AsyncTaskRunner", "Contract violation: worker returned Result::cancelled after task was skipped"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Cancelled,
                                    result.message().isEmpty() ? QCoreApplication::translate("AsyncTaskRunner", "Cancelled") : result.message());
                            } else { // result.isSkipped() -> Agreement
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Skipped,
                                    result.message().isEmpty() ? QCoreApplication::translate("AsyncTaskRunner", "Skipped") : result.message());
                            }

                        } else {
                            // Normal / Completed / Running state -> outcome determined by Result
                            if (result.isSuccess()) {
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Completed,
                                    result.message().isEmpty() ? QCoreApplication::translate("AsyncTaskRunner", "Completed") : result.message());
                            } else if (result.isCancelled()) {
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Cancelled,
                                    result.message().isEmpty() ? QCoreApplication::translate("AsyncTaskRunner", "Cancelled") : result.message());
                            } else if (result.isSkipped()) {
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Skipped,
                                    result.message().isEmpty() ? QCoreApplication::translate("AsyncTaskRunner", "Skipped") : result.message());
                            } else { // Failure
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Failed,
                                    result.message().isEmpty() ? QCoreApplication::translate("AsyncTaskRunner", "Task failed") : result.message());
                            }
                        }
                    }
                }

                if constexpr (std::is_invocable_v<DecayedCallback, Result<T>>) {
                    if (hasValidCallback) {
                        if (contextGuard) {
                            QMetaObject::invokeMethod(contextGuard.data(), [contextGuard, cb = std::move(callback), res = std::move(result)]() {
                                try {
                                    if (contextGuard && Detail::isCallableValid(cb)) {
                                        cb(res);
                                    }
                                } catch (...) {}
                            }, Qt::QueuedConnection);
                        } else if (!context) {
                            try {
                                callback(result);
                            } catch (...) {}
                        }
                    }
                }
            } catch (...) {
                // Guaranteed no unhandled exception ever leaks to QThreadPool
            }
        };

        QRunnable* runnable = QRunnable::create(std::move(workerLambda));
        if (pool) {
            pool->start(runnable);
        } else {
            QThreadPool::globalInstance()->start(runnable);
        }
        return handle;
    }
};

} // namespace Application::Async
