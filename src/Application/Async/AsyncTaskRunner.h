#pragma once

#include <QObject>
#include <QPointer>
#include <QThreadPool>
#include <QMetaObject>
#include <QString>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Application/Async/TaskResult.h"

namespace Application::Async {

namespace Detail {

template <typename T>
struct is_std_function : std::false_type {};

template <typename Ret, typename... Args>
struct is_std_function<std::function<Ret(Args...)>> : std::true_type {};

template <typename T>
inline constexpr bool is_std_function_v = is_std_function<std::decay_t<T>>::value;

template <typename F>
bool isCallableValid(const F& f)
{
    if constexpr (std::is_same_v<std::decay_t<F>, std::nullptr_t>) {
        return false;
    } else if constexpr (std::is_pointer_v<std::decay_t<F>>) {
        return f != nullptr;
    } else if constexpr (is_std_function_v<F>) {
        return static_cast<bool>(f);
    } else {
        return true;
    }
}

template <typename T>
struct is_optional_type : std::false_type {};

template <typename T>
struct is_optional_type<std::optional<T>> : std::true_type {};

template <typename T>
struct is_task_result_type : std::false_type {};

template <typename T>
struct is_task_result_type<TaskResult<T>> : std::true_type {};

template <typename ResultType>
TaskExecutionStatus inspectResultStatus(const ResultType& result)
{
    if constexpr (is_task_result_type<std::decay_t<ResultType>>::value) {
        return result.status();
    } else if constexpr (std::is_same_v<std::decay_t<ResultType>, bool>) {
        return result ? TaskExecutionStatus::Success : TaskExecutionStatus::Failure;
    } else if constexpr (is_optional_type<std::decay_t<ResultType>>::value) {
        return result.has_value() ? TaskExecutionStatus::Success : TaskExecutionStatus::Failure;
    } else {
        return TaskExecutionStatus::Success;
    }
}

} // namespace Detail

/**
 * @brief Helper for standardized asynchronous task execution with TaskLoggingContext,
 * multi-level hierarchical sub-tasks, exception safety, semantic failure detection, and Qt thread-safe callback delivery.
 *
 * Contract & Guarantees:
 * - Worker Execution: Always executed in a worker thread (via QThreadPool).
 * - Lifecycle Management: TaskLoggingContext is automatically created and started.
 * - Semantic Outcome Handling (TaskResult<T> / optional / bool):
 *   1. An unhandled exception -> TaskState::Failed.
 *   2. TaskResult::success(...) (or valid optional / true) with 0 logged errors -> TaskState::Completed.
 *   3. TaskResult::failure(...) (or nullopt / false / logged errors) -> TaskState::Failed.
 *   4. TaskResult::cancelled(...) -> TaskState::Cancelled.
 *   5. TaskResult::skipped(...) -> TaskState::Completed.
 *   6. Explicit worker context call (taskContext->fail/complete/cancel) -> Explicit terminal state preserved.
 * - Context Affinity: @p context is optional (nullptr allowed for pure background/headless tasks).
 * - Callback Delivery: If @p context != nullptr and @p callback is valid/callable, results are
 *   delivered to @p context's thread via Qt::QueuedConnection.
 * - Lifetime Guarding: If @p context is destroyed before worker finishes, callback is safely dropped.
 * - Fire-and-Forget: If @p callback is omitted, null, or empty, worker runs and logs normally in the background.
 */
class AsyncTaskRunner {
public:
    /**
     * @brief Runs an async worker with a return value and delivers the result to callback on context thread.
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
        if (taskContext) {
            taskContext->start();
        }

        QPointer<QObject> contextGuard(context);
        quint64 taskId = taskContext ? taskContext->taskId() : 0;

        auto workerLambda = [taskContext, taskId, contextGuard,
                             worker = std::forward<WorkerFn>(worker),
                             callback = std::forward<CallbackFn>(callback)]() mutable {
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
                        TaskExecutionStatus execStatus = Detail::inspectResultStatus(result);
                        if (execStatus == TaskExecutionStatus::Success) {
                            if (taskContext->hasErrors()) {
                                Core::Logging::LogManager::instance().failTask(taskId, QStringLiteral("Task completed with logged errors"));
                            } else {
                                Core::Logging::LogManager::instance().finishTask(taskId, QStringLiteral("Completed"));
                            }
                        } else if (execStatus == TaskExecutionStatus::Cancelled) {
                            Core::Logging::LogManager::instance().cancelTask(taskId, QStringLiteral("Cancelled"));
                        } else if (execStatus == TaskExecutionStatus::Skipped) {
                            Core::Logging::LogManager::instance().finishTask(taskId, QStringLiteral("Skipped"));
                        } else {
                            Core::Logging::LogManager::instance().failTask(taskId, QStringLiteral("Task failed"));
                        }
                    }
                } else {
                    Core::Logging::LogManager::instance().flushTask(taskId);
                }
            }

            if constexpr (std::is_invocable_v<CallbackFn, ResultType>) {
                if (Detail::isCallableValid(callback) && contextGuard) {
                    QMetaObject::invokeMethod(contextGuard.data(), [contextGuard, callback = std::move(callback), res = std::move(result)]() {
                        if (contextGuard) {
                            callback(res);
                        }
                    }, Qt::QueuedConnection);
                }
            }
        };

        if (pool) {
            pool->start(std::move(workerLambda));
        } else {
            QThreadPool::globalInstance()->start(std::move(workerLambda));
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
        if (taskContext) {
            taskContext->start();
        }

        QPointer<QObject> contextGuard(context);
        quint64 taskId = taskContext ? taskContext->taskId() : 0;

        auto workerLambda = [taskContext, taskId, contextGuard,
                             worker = std::forward<WorkerFn>(worker),
                             callback = std::forward<CallbackFn>(callback)]() mutable {
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
                if (Detail::isCallableValid(callback) && contextGuard) {
                    QMetaObject::invokeMethod(contextGuard.data(), [contextGuard, callback = std::move(callback)]() {
                        if (contextGuard) {
                            callback();
                        }
                    }, Qt::QueuedConnection);
                }
            }
        };

        if (pool) {
            pool->start(std::move(workerLambda));
        } else {
            QThreadPool::globalInstance()->start(std::move(workerLambda));
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
