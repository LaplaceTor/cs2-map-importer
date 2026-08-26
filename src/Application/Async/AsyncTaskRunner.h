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

namespace Application::Async {

using Core::Async::TaskResult;
using Core::Async::TaskExecutionStatus;

namespace Detail {

template <typename T>
struct is_optional_type : std::false_type {};

template <typename T>
struct is_optional_type<std::optional<T>> : std::true_type {};

template <typename T>
struct is_task_result_type : std::false_type {};

template <typename T>
struct is_task_result_type<Core::Async::TaskResult<T>> : std::true_type {};

template <typename T>
struct is_task_result_type<const Core::Async::TaskResult<T>&> : std::true_type {};

template <typename Fn>
bool isCallableValid(const Fn& fn) {
    if constexpr (std::is_convertible_v<Fn, bool>) {
        return !!fn;
    } else {
        return true;
    }
}

template <typename ResultType>
Core::Async::TaskExecutionStatus inspectResultStatus(const ResultType& result) {
    if constexpr (is_task_result_type<std::decay_t<ResultType>>::value) {
        return result.status();
    } else if constexpr (std::is_same_v<std::decay_t<ResultType>, bool>) {
        return result ? Core::Async::TaskExecutionStatus::Success : Core::Async::TaskExecutionStatus::Failure;
    } else if constexpr (is_optional_type<std::decay_t<ResultType>>::value) {
        return result.has_value() ? Core::Async::TaskExecutionStatus::Success : Core::Async::TaskExecutionStatus::Failure;
    } else {
        return Core::Async::TaskExecutionStatus::Success;
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
 *    - Produced by the worker and returned to the caller callback (`Success`, `Failure`, `Cancelled`, `Skipped` + payload `T`).
 *
 * Contract & Guarantees:
 * - Workflow and Application code must use `TaskResult<T>` via `runTask<T>()` or `runChildTask<T>()`.
 * - If task creation is rejected (e.g. invalid parentTaskId), workers are strictly NOT dispatched.
 * - An explicit `TaskResult<T>::failure` is safely delivered to the callback.
 * - AsyncTaskRunner NEVER guesses or synthesizes fake default values for arbitrary non-TaskResult types.
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
    template <typename T, typename WorkerFn, typename CallbackFn = std::function<void(const TaskResult<T>&)>>
    static void runTask(
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback = CallbackFn{},
        QThreadPool* pool = QThreadPool::globalInstance(),
        quint64 parentTaskId = 0)
    {
        run<TaskResult<T>>(taskName, context, std::forward<WorkerFn>(worker), std::forward<CallbackFn>(callback), pool, parentTaskId);
    }

    /**
     * @brief Primary API: Runs an async child sub-task whose business outcome is TaskResult<T>.
     */
    template <typename T, typename WorkerFn, typename CallbackFn = std::function<void(const TaskResult<T>&)>>
    static void runChildTask(
        quint64 parentTaskId,
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback = CallbackFn{},
        QThreadPool* pool = QThreadPool::globalInstance())
    {
        run<TaskResult<T>>(taskName, context, std::forward<WorkerFn>(worker), std::forward<CallbackFn>(callback), pool, parentTaskId);
    }

    /**
     * @brief Runs an async worker with an explicit ResultType and delivers the result to callback on context thread.
     */
    template <typename ResultType, typename WorkerFn, typename CallbackFn = std::function<void(const ResultType&)>>
    static void run(
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback = CallbackFn{},
        QThreadPool* pool = QThreadPool::globalInstance(),
        quint64 parentTaskId = 0)
    {
        auto taskContext = Core::Logging::LogManager::instance().createTask(taskName, parentTaskId);
        if (!taskContext) {
            // Task creation rejected (e.g. invalid parentTaskId). Do NOT dispatch worker.
            // Explicitly deliver failure if ResultType is a TaskResult<T>.
            // Never guess or deliver fake default-constructed values for arbitrary types.
            if constexpr (Detail::is_task_result_type<std::decay_t<ResultType>>::value) {
                if (Detail::isCallableValid(callback)) {
                    ResultType failureResult = ResultType::failure(
                        QStringLiteral("Failed to create task context for '%1' (invalid parentTaskId: %2)")
                            .arg(taskName).arg(parentTaskId));
                    if (context) {
                        QPointer<QObject> guard(context);
                        QMetaObject::invokeMethod(guard.data(), [guard, cb = std::forward<CallbackFn>(callback), res = std::move(failureResult)]() {
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
        if constexpr (std::is_invocable_v<CallbackFn, ResultType>) {
            hasValidCallback = Detail::isCallableValid(callback);
        }

        auto workerLambda = [taskContext, taskId, contextGuard, context, hasValidCallback,
                             worker = std::forward<WorkerFn>(worker),
                             callback = std::forward<CallbackFn>(callback)]() mutable {
            try {
                ResultType result{};
                bool threwException = false;

                try {
                    result = worker(taskContext);
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
                    bool isTerminal = Core::Logging::TaskLoggingContext::isTerminalState(taskContext->state());
                    if (!isTerminal) {
                        if (threwException) {
                            Core::Logging::LogManager::instance().failTask(taskId, QStringLiteral("Task failed with exception"));
                        } else {
                            Core::Async::TaskExecutionStatus execStatus = Detail::inspectResultStatus(result);
                            if (execStatus == Core::Async::TaskExecutionStatus::Success) {
                                if (taskContext->hasErrors()) {
                                    Core::Logging::LogManager::instance().failTask(taskId, QStringLiteral("Task completed with logged errors"));
                                } else {
                                    Core::Logging::LogManager::instance().finishTask(taskId, QStringLiteral("Completed"));
                                }
                            } else if (execStatus == Core::Async::TaskExecutionStatus::Cancelled) {
                                Core::Logging::LogManager::instance().cancelTask(taskId, QStringLiteral("Cancelled"));
                            } else if (execStatus == Core::Async::TaskExecutionStatus::Skipped) {
                                Core::Logging::LogManager::instance().skipTask(taskId, QStringLiteral("Skipped"));
                            } else {
                                Core::Logging::LogManager::instance().failTask(taskId, QStringLiteral("Task failed"));
                            }
                        }
                    } else {
                        Core::Logging::LogManager::instance().flushTask(taskId);
                    }
                }

                if constexpr (std::is_invocable_v<CallbackFn, ResultType>) {
                    if (hasValidCallback) {
                        if (contextGuard) {
                            QMetaObject::invokeMethod(contextGuard.data(), [contextGuard, callback = std::move(callback), res = std::move(result)]() {
                                try {
                                    if (contextGuard && Detail::isCallableValid(callback)) {
                                        callback(res);
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
     * @brief Runs an async worker without a return value.
     */
    template <typename WorkerFn, typename CallbackFn = std::function<void()>>
    static void runVoid(
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback = CallbackFn{},
        QThreadPool* pool = QThreadPool::globalInstance(),
        quint64 parentTaskId = 0)
    {
        auto taskContext = Core::Logging::LogManager::instance().createTask(taskName, parentTaskId);
        if (!taskContext) {
            // Task creation rejected (e.g. invalid parentTaskId). Do NOT dispatch worker.
            return;
        }

        taskContext->start();
        QPointer<QObject> contextGuard(context);
        quint64 taskId = taskContext->taskId();
        bool hasValidCallback = false;
        if constexpr (std::is_invocable_v<CallbackFn>) {
            hasValidCallback = Detail::isCallableValid(callback);
        }

        auto workerLambda = [taskContext, taskId, contextGuard, context, hasValidCallback,
                             worker = std::forward<WorkerFn>(worker),
                             callback = std::forward<CallbackFn>(callback)]() mutable {
            try {
                bool threwException = false;

                try {
                    worker(taskContext);
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
                    bool isTerminal = Core::Logging::TaskLoggingContext::isTerminalState(taskContext->state());
                    if (!isTerminal) {
                        if (threwException || taskContext->hasErrors()) {
                            Core::Logging::LogManager::instance().failTask(taskId, QStringLiteral("Task failed"));
                        } else {
                            Core::Logging::LogManager::instance().finishTask(taskId, QStringLiteral("Completed"));
                        }
                    } else {
                        Core::Logging::LogManager::instance().flushTask(taskId);
                    }
                }

                if constexpr (std::is_invocable_v<CallbackFn>) {
                    if (hasValidCallback) {
                        if (contextGuard) {
                            QMetaObject::invokeMethod(contextGuard.data(), [contextGuard, callback = std::move(callback)]() {
                                try {
                                    if (contextGuard && Detail::isCallableValid(callback)) {
                                        callback();
                                    }
                                } catch (...) {}
                            }, Qt::QueuedConnection);
                        } else if (!context) {
                            try {
                                callback();
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
     * @brief Runs a pure background/fire-and-forget worker task without context or callback.
     */
    template <typename WorkerFn>
    static void runBackground(
        const QString& taskName,
        WorkerFn&& worker,
        QThreadPool* pool = QThreadPool::globalInstance(),
        quint64 parentTaskId = 0)
    {
        runVoid(taskName, nullptr, std::forward<WorkerFn>(worker), std::function<void()>{}, pool, parentTaskId);
    }

    /**
     * @brief Convenience helper to run a child sub-task attached to parentTaskId.
     */
    template <typename ResultType, typename WorkerFn, typename CallbackFn = std::function<void(const ResultType&)>>
    static void runChild(
        quint64 parentTaskId,
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback = CallbackFn{},
        QThreadPool* pool = QThreadPool::globalInstance())
    {
        run<ResultType>(taskName, context, std::forward<WorkerFn>(worker), std::forward<CallbackFn>(callback), pool, parentTaskId);
    }

    /**
     * @brief Convenience helper to run a child void sub-task attached to parentTaskId.
     */
    template <typename WorkerFn, typename CallbackFn = std::function<void()>>
    static void runChildVoid(
        quint64 parentTaskId,
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback = CallbackFn{},
        QThreadPool* pool = QThreadPool::globalInstance())
    {
        runVoid(taskName, context, std::forward<WorkerFn>(worker), std::forward<CallbackFn>(callback), pool, parentTaskId);
    }
};

} // namespace Application::Async
