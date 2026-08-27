#pragma once

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
    static void runTask(
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback = CallbackFn{},
        QThreadPool* pool = QThreadPool::globalInstance(),
        quint64 parentTaskId = 0)
    {
        runTaskInternal<T>(taskName, context, std::forward<WorkerFn>(worker), std::forward<CallbackFn>(callback), pool, parentTaskId);
    }

    /**
     * @brief Primary API: Runs an async child sub-task whose business outcome is Result<T>.
     *
     * @tparam T The business payload type (e.g. GameInstallationInfo, DetectionResult, or void).
     * @param parentTaskId Parent task ID in LogManager.
     * @param taskName The name of the child sub-task.
     * @param context The Qt lifetime context object.
     * @param worker Lambda taking std::shared_ptr<TaskLoggingContext> and returning Result<T>.
     * @param callback Callback receiving const Result<T>&.
     * @param pool The QThreadPool to dispatch to.
     */
    template <typename T = void, typename WorkerFn, typename CallbackFn = std::function<void(const Result<T>&)>>
    static void runChildTask(
        quint64 parentTaskId,
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback = CallbackFn{},
        QThreadPool* pool = QThreadPool::globalInstance())
    {
        runTaskInternal<T>(taskName, context, std::forward<WorkerFn>(worker), std::forward<CallbackFn>(callback), pool, parentTaskId);
    }

    /**
     * @brief Runs a fire-and-forget background worker task without return value or UI callback.
     *
     * Convenience wrapper delegating to runTask<void> to ensure unified execution lifecycle,
     * structured error handling, and state arbitration across all tasks.
     */
    template <typename WorkerFn>
    static void runBackground(
        const QString& taskName,
        WorkerFn&& worker,
        QThreadPool* pool = QThreadPool::globalInstance(),
        quint64 parentTaskId = 0)
    {
        using DecayedWorker = std::decay_t<WorkerFn>;
        if constexpr (std::is_invocable_r_v<Result<void>, DecayedWorker, std::shared_ptr<Core::Logging::TaskLoggingContext>>) {
            runTask<void>(taskName, nullptr, std::forward<WorkerFn>(worker), {}, pool, parentTaskId);
        } else {
            runTask<void>(
                taskName,
                nullptr,
                [w = DecayedWorker(std::forward<WorkerFn>(worker))](std::shared_ptr<Core::Logging::TaskLoggingContext> ctx) mutable -> Result<void> {
                    w(ctx);
                    return Result<void>::success();
                },
                {},
                pool,
                parentTaskId);
        }
    }

private:
    /**
     * @brief Internal engine executing a typed Result<T> async worker.
     */
    template <typename T, typename WorkerFn, typename CallbackFn>
    static void runTaskInternal(
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback,
        QThreadPool* pool,
        quint64 parentTaskId)
    {
        using DecayedWorker = std::decay_t<WorkerFn>;
        using DecayedCallback = std::decay_t<CallbackFn>;

        auto taskContext = Core::Logging::LogManager::instance().createTask(taskName, parentTaskId);
        if (!taskContext) {
            // Task creation rejected (e.g. invalid parentTaskId). Do NOT dispatch worker.
            // Safely deliver explicit Result<T>::failure to callback.
            if constexpr (std::is_invocable_v<DecayedCallback, Result<T>>) {
                if (Detail::isCallableValid(callback)) {
                    Result<T> failureResult = Result<T>::failure(
                        Core::Error::ErrorCode::OperationFailed,
                        QStringLiteral("Failed to create task context for '%1' (invalid parentTaskId: %2)")
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
            return;
        }

        taskContext->start();
        QPointer<QObject> contextGuard(context);
        quint64 taskId = taskContext->taskId();
        bool hasValidCallback = false;
        if constexpr (std::is_invocable_v<DecayedCallback, Result<T>>) {
            hasValidCallback = Detail::isCallableValid(callback);
        }

        auto workerLambda = [taskContext, taskId, taskName, contextGuard, context, hasValidCallback,
                             worker = DecayedWorker(std::forward<WorkerFn>(worker)),
                             callback = DecayedCallback(std::forward<CallbackFn>(callback))]() mutable {
            try {
                Result<T> result{};
                bool threwException = false;

                try {
                    result = worker(taskContext);
                } catch (const Core::Error::Exception& ex) {
                    threwException = true;
                    if (taskContext) {
                        const QString detailInfo = ex.details().isEmpty()
                            ? (ex.message().isEmpty() ? QString::fromUtf8(ex.what()) : ex.message())
                            : QStringLiteral("%1 (%2)").arg(ex.message().isEmpty() ? QString::fromUtf8(ex.what()) : ex.message(), ex.details());
                        taskContext->error(QStringLiteral("Task exception [%1]: %2")
                            .arg(static_cast<int>(ex.errorCode()))
                            .arg(detailInfo));
                    }
                    result = Result<T>::failure(
                        ex.error(),
                        QStringLiteral("Task '%1' failed").arg(taskName));
                } catch (const std::exception& ex) {
                    threwException = true;
                    if (taskContext) {
                        taskContext->error(QStringLiteral("Unhandled standard exception: %1").arg(QString::fromUtf8(ex.what())));
                    }
                    result = Result<T>::failure(
                        Core::Error::Error::unknown(
                            QStringLiteral("Unhandled standard exception"),
                            QString::fromUtf8(ex.what())),
                        QStringLiteral("Task '%1' failed").arg(taskName));
                } catch (...) {
                    threwException = true;
                    if (taskContext) {
                        taskContext->error(QStringLiteral("Unhandled unknown exception in task"));
                    }
                    result = Result<T>::failure(
                        Core::Error::Error::unknown(QStringLiteral("Unhandled unknown exception")),
                        QStringLiteral("Task '%1' failed").arg(taskName));
                }

                if (taskContext) {
                    if (threwException) {
                        QString taskSummary = QStringLiteral("Task failed with uncaught exception");
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
                                    taskContext->error(QStringLiteral("Contract violation: worker returned Result::success after task failed"));
                                }
                                result = makeContractFailure(QStringLiteral("Contract violation: Task completed with logged errors or explicit failure"));
                            } else if (result.isCancelled()) {
                                taskContext->error(QStringLiteral("Contract violation: worker returned Result::cancelled after task failed with errors"));
                                result = makeContractFailure(QStringLiteral("Contract violation: Task failed with errors before cancellation"));
                            } else if (result.isSkipped()) {
                                taskContext->error(QStringLiteral("Contract violation: worker returned Result::skipped after task failed with errors"));
                                result = makeContractFailure(QStringLiteral("Contract violation: Task failed with errors before skipping"));
                            }
                            Core::Logging::LogManager::instance().forceTaskState(
                                taskId, Core::Logging::TaskState::Failed,
                                result.message().isEmpty() ? QStringLiteral("Task failed") : result.message());

                        } else if (currentState == Core::Logging::TaskState::Cancelled) {
                            // Priority 2: Cancelled (without errors)
                            if (result.isSuccess()) {
                                taskContext->warning(QStringLiteral("Contract violation: worker returned Result::success after task was cancelled"));
                                result = makeContractCancelled(QStringLiteral("Contract violation: Task was cancelled"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Cancelled, QStringLiteral("Cancelled"));
                            } else if (result.isFailure()) {
                                taskContext->warning(QStringLiteral("Contract violation: worker returned Result::failure after task was cancelled"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Failed,
                                    result.message().isEmpty() ? QStringLiteral("Task failed") : result.message());
                            } else if (result.isSkipped()) {
                                taskContext->warning(QStringLiteral("Contract violation: worker returned Result::skipped after task was cancelled"));
                                result = makeContractCancelled(QStringLiteral("Contract violation: Task was cancelled"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Cancelled, QStringLiteral("Cancelled"));
                            } else { // result.isCancelled() -> Agreement
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Cancelled,
                                    result.message().isEmpty() ? QStringLiteral("Cancelled") : result.message());
                            }

                        } else if (currentState == Core::Logging::TaskState::Skipped) {
                            // Priority 3: Skipped (without errors)
                            if (result.isSuccess()) {
                                taskContext->warning(QStringLiteral("Contract violation: worker returned Result::success after task was skipped"));
                                result = makeContractSkipped(QStringLiteral("Contract violation: Task was skipped"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Skipped, QStringLiteral("Skipped"));
                            } else if (result.isFailure()) {
                                taskContext->warning(QStringLiteral("Contract violation: worker returned Result::failure after task was skipped"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Failed,
                                    result.message().isEmpty() ? QStringLiteral("Task failed") : result.message());
                            } else if (result.isCancelled()) {
                                taskContext->warning(QStringLiteral("Contract violation: worker returned Result::cancelled after task was skipped"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Cancelled,
                                    result.message().isEmpty() ? QStringLiteral("Cancelled") : result.message());
                            } else { // result.isSkipped() -> Agreement
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Skipped,
                                    result.message().isEmpty() ? QStringLiteral("Skipped") : result.message());
                            }

                        } else {
                            // Normal / Completed / Running state -> outcome determined by Result
                            if (result.isSuccess()) {
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Completed,
                                    result.message().isEmpty() ? QStringLiteral("Completed") : result.message());
                            } else if (result.isCancelled()) {
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Cancelled,
                                    result.message().isEmpty() ? QStringLiteral("Cancelled") : result.message());
                            } else if (result.isSkipped()) {
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Skipped,
                                    result.message().isEmpty() ? QStringLiteral("Skipped") : result.message());
                            } else { // Failure
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Failed,
                                    result.message().isEmpty() ? QStringLiteral("Task failed") : result.message());
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
    }
};

} // namespace Application::Async
