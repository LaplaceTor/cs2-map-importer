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
#include "Core/Async/TaskResult.h"
#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Error/Exception.h"

namespace Application::Async {

using Core::Async::TaskResult;
using Core::Async::TaskExecutionStatus;

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
 * (Core::Logging::TaskState) and Business Outcome (Core::Async::TaskResult<T>).
 *
 * Architectural Dual-Plane Model:
 * 1. **Execution Lifecycle Plane (`TaskState`)**:
 *    - Managed by `LogManager` / `TaskLoggingContext` (`Pending` -> `Running` -> `Completed` | `Failed` | `Cancelled` | `Skipped`).
 *    - Tracked and rendered in UI log models (`LogViewModel`, `LogTaskModel`).
 * 2. **Business Outcome Plane (`TaskResult<T>`)**:
 *    - Standard single-layer return contract for Workflow and Application APIs (`Success`, `Failure`, `Cancelled`, `Skipped` + payload `T`).
 *
 * Public Business API:
 * - `runTask<T>(taskName, context, worker, callback)`: For tasks with business payload `T`.
 * - `runTask<void>(taskName, context, worker, callback)`: For tasks without payload, retaining full `TaskResult<void>` outcome semantics.
 * - `runChildTask<T>(parentTaskId, taskName, context, worker, callback)`: For hierarchical child sub-tasks.
 * - `runChildTask<void>(parentTaskId, taskName, context, worker, callback)`: For void child sub-tasks.
 * - `runBackground(taskName, worker)`: For fire-and-forget background logging tasks.
 */
class AsyncTaskRunner {
public:
    /**
     * @brief Primary API: Runs an async task whose business outcome is TaskResult<T>.
     *
     * @tparam T The business payload type (e.g. GameInstallationInfo, DetectionResult, or void).
     * @param taskName The name of the task for logging/UI display.
     * @param context The Qt lifetime context object (callback marshaled to its thread).
     * @param worker Lambda taking std::shared_ptr<TaskLoggingContext> and returning TaskResult<T>.
     * @param callback Callback receiving const TaskResult<T>&.
     * @param pool The QThreadPool to dispatch to (defaults to globalInstance).
     * @param parentTaskId Optional parent task ID for hierarchical sub-tasks.
     */
    template <typename T = void, typename WorkerFn, typename CallbackFn = std::function<void(const TaskResult<T>&)>>
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
     * @brief Primary API: Runs an async child sub-task whose business outcome is TaskResult<T>.
     *
     * @tparam T The business payload type (e.g. GameInstallationInfo, DetectionResult, or void).
     * @param parentTaskId Parent task ID in LogManager.
     * @param taskName The name of the child sub-task.
     * @param context The Qt lifetime context object.
     * @param worker Lambda taking std::shared_ptr<TaskLoggingContext> and returning TaskResult<T>.
     * @param callback Callback receiving const TaskResult<T>&.
     * @param pool The QThreadPool to dispatch to.
     */
    template <typename T = void, typename WorkerFn, typename CallbackFn = std::function<void(const TaskResult<T>&)>>
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
     */
    template <typename WorkerFn>
    static void runBackground(
        const QString& taskName,
        WorkerFn&& worker,
        QThreadPool* pool = QThreadPool::globalInstance(),
        quint64 parentTaskId = 0)
    {
        runVoidInternal(taskName, std::forward<WorkerFn>(worker), pool, parentTaskId);
    }

private:
    /**
     * @brief Internal engine executing a typed TaskResult<T> async worker.
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
            // Safely deliver explicit TaskResult<T>::failure to callback.
            if constexpr (std::is_invocable_v<DecayedCallback, TaskResult<T>>) {
                if (Detail::isCallableValid(callback)) {
                    TaskResult<T> failureResult = TaskResult<T>::failure(
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
        if constexpr (std::is_invocable_v<DecayedCallback, TaskResult<T>>) {
            hasValidCallback = Detail::isCallableValid(callback);
        }

        auto workerLambda = [taskContext, taskId, contextGuard, context, hasValidCallback,
                             worker = DecayedWorker(std::forward<WorkerFn>(worker)),
                             callback = DecayedCallback(std::forward<CallbackFn>(callback))]() mutable {
            try {
                TaskResult<T> result{};
                bool threwException = false;

                try {
                    result = worker(taskContext);
                } catch (const Core::Error::Exception& ex) {
                    threwException = true;
                    if (taskContext) {
                        taskContext->error(QStringLiteral("Task exception [%1]: %2")
                            .arg(static_cast<int>(ex.errorCode()))
                            .arg(ex.message().isEmpty() ? QString::fromUtf8(ex.what()) : ex.message()));
                    }
                    result = TaskResult<T>::failure(ex.error());
                } catch (const std::exception& ex) {
                    threwException = true;
                    if (taskContext) {
                        taskContext->error(QStringLiteral("Unhandled exception in task: %1").arg(QString::fromUtf8(ex.what())));
                    }
                    result = TaskResult<T>::failure(
                        Core::Error::ErrorCode::Unknown,
                        QStringLiteral("Unhandled standard exception"),
                        QString::fromUtf8(ex.what()));
                } catch (...) {
                    threwException = true;
                    if (taskContext) {
                        taskContext->error(QStringLiteral("Unhandled unknown exception in task"));
                    }
                    result = TaskResult<T>::failure(Core::Error::ErrorCode::Unknown, QStringLiteral("Unhandled unknown exception"));
                }

                if (taskContext) {
                    if (threwException) {
                        Core::Logging::LogManager::instance().forceTaskState(
                            taskId, Core::Logging::TaskState::Failed, result.message().isEmpty() ? QStringLiteral("Task failed with uncaught exception") : result.message());
                    } else {
                        const auto currentState = taskContext->state();
                        const bool hasErrors = taskContext->hasErrors();

                        if (result.isSuccess()) {
                            if (currentState == Core::Logging::TaskState::Failed || hasErrors) {
                                if (!hasErrors) {
                                    taskContext->error(QStringLiteral("Contract violation: worker returned TaskResult::success after task failed"));
                                }
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Failed, QStringLiteral("Task completed with logged errors or explicit failure"));

                                if constexpr (std::is_void_v<T>) {
                                    result = TaskResult<T>::failure(
                                        Core::Error::ErrorCode::OperationFailed,
                                        QStringLiteral("Contract violation: Task completed with logged errors or explicit failure"));
                                } else {
                                    result = TaskResult<T>::failure(
                                        Core::Error::ErrorCode::OperationFailed,
                                        QStringLiteral("Contract violation: Task completed with logged errors or explicit failure"),
                                        QString(),
                                        result.hasValue() ? std::make_optional(result.value()) : std::nullopt);
                                }
                            } else if (currentState == Core::Logging::TaskState::Cancelled) {
                                taskContext->warning(QStringLiteral("Contract violation: worker returned TaskResult::success after task was cancelled"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Cancelled, QStringLiteral("Cancelled"));

                                if constexpr (std::is_void_v<T>) {
                                    result = TaskResult<T>::cancelled(QStringLiteral("Contract violation: Task was cancelled"));
                                } else {
                                    result = TaskResult<T>::cancelled(
                                        QStringLiteral("Contract violation: Task was cancelled"),
                                        result.hasValue() ? std::make_optional(result.value()) : std::nullopt);
                                }
                            } else if (currentState == Core::Logging::TaskState::Skipped) {
                                taskContext->info(QStringLiteral("Contract violation: worker returned TaskResult::success after task was skipped"));
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Skipped, QStringLiteral("Skipped"));

                                if constexpr (std::is_void_v<T>) {
                                    result = TaskResult<T>::skipped(QStringLiteral("Contract violation: Task was skipped"));
                                } else {
                                    result = TaskResult<T>::skipped(
                                        QStringLiteral("Contract violation: Task was skipped"),
                                        result.hasValue() ? std::make_optional(result.value()) : std::nullopt);
                                }
                            } else {
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Completed, QStringLiteral("Completed"));
                            }
                        } else if (result.isCancelled()) {
                            if (hasErrors || currentState == Core::Logging::TaskState::Failed) {
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Failed, QStringLiteral("Task cancelled with logged errors"));
                            } else {
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Cancelled, result.message().isEmpty() ? QStringLiteral("Cancelled") : result.message());
                            }
                        } else if (result.isSkipped()) {
                            if (hasErrors || currentState == Core::Logging::TaskState::Failed) {
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Failed, QStringLiteral("Task skipped with logged errors"));
                            } else {
                                Core::Logging::LogManager::instance().forceTaskState(
                                    taskId, Core::Logging::TaskState::Skipped, result.message().isEmpty() ? QStringLiteral("Skipped") : result.message());
                            }
                        } else {
                            Core::Logging::LogManager::instance().forceTaskState(
                                taskId, Core::Logging::TaskState::Failed, result.message().isEmpty() ? QStringLiteral("Task failed") : result.message());
                        }
                    }
                }

                if constexpr (std::is_invocable_v<DecayedCallback, TaskResult<T>>) {
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

    /**
     * @brief Internal engine for fire-and-forget void background operations.
     */
    template <typename WorkerFn>
    static void runVoidInternal(
        const QString& taskName,
        WorkerFn&& worker,
        QThreadPool* pool,
        quint64 parentTaskId)
    {
        using DecayedWorker = std::decay_t<WorkerFn>;

        auto taskContext = Core::Logging::LogManager::instance().createTask(taskName, parentTaskId);
        if (!taskContext) {
            return;
        }

        taskContext->start();
        quint64 taskId = taskContext->taskId();

        auto workerLambda = [taskContext, taskId,
                             worker = DecayedWorker(std::forward<WorkerFn>(worker))]() mutable {
            try {
                bool threwException = false;

                try {
                    worker(taskContext);
                } catch (const Core::Error::Exception& ex) {
                    threwException = true;
                    if (taskContext) {
                        taskContext->error(QStringLiteral("Task exception [%1]: %2")
                            .arg(static_cast<int>(ex.errorCode()))
                            .arg(ex.message().isEmpty() ? QString::fromUtf8(ex.what()) : ex.message()));
                    }
                } catch (const std::exception& ex) {
                    threwException = true;
                    if (taskContext) {
                        taskContext->error(QStringLiteral("Unhandled exception in task: %1").arg(QString::fromUtf8(ex.what())));
                    }
                } catch (...) {
                    threwException = true;
                    if (taskContext) {
                        taskContext->error(QStringLiteral("Unhandled unknown exception in task"));
                    }
                }

                if (taskContext) {
                    if (threwException) {
                        Core::Logging::LogManager::instance().forceTaskState(
                            taskId, Core::Logging::TaskState::Failed, QStringLiteral("Task failed with uncaught exception"));
                    } else {
                        const auto currentState = taskContext->state();
                        if (taskContext->hasErrors() || currentState == Core::Logging::TaskState::Failed) {
                            Core::Logging::LogManager::instance().forceTaskState(
                                taskId, Core::Logging::TaskState::Failed, QStringLiteral("Task failed"));
                        } else if (currentState == Core::Logging::TaskState::Cancelled) {
                            Core::Logging::LogManager::instance().forceTaskState(
                                taskId, Core::Logging::TaskState::Cancelled, QStringLiteral("Cancelled"));
                        } else if (currentState == Core::Logging::TaskState::Skipped) {
                            Core::Logging::LogManager::instance().forceTaskState(
                                taskId, Core::Logging::TaskState::Skipped, QStringLiteral("Skipped"));
                        } else {
                            Core::Logging::LogManager::instance().forceTaskState(
                                taskId, Core::Logging::TaskState::Completed, QStringLiteral("Completed"));
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
