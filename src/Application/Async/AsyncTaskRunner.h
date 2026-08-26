#pragma once

#include <QObject>
#include <QPointer>
#include <QThreadPool>
#include <QMetaObject>
#include <QString>
#include <exception>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskLoggingContext.h"

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

} // namespace Detail

/**
 * @brief Helper for standardized asynchronous task execution with TaskLoggingContext,
 * multi-level hierarchical sub-tasks, exception safety, and Qt thread-safe callback delivery.
 *
 * Contract & Guarantees:
 * - Worker Execution: Always executed in a worker thread (via QThreadPool).
 * - Lifecycle Management: TaskLoggingContext is automatically created, started, and cleanly
 *   finished/failed in LogManager upon worker completion or uncaught exception.
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
            bool succeeded = false;

            try {
                result = worker(taskContext);
                succeeded = true;
            } catch (const std::exception& ex) {
                if (taskContext) {
                    taskContext->error(QStringLiteral("Unhandled exception in task: %1").arg(QString::fromUtf8(ex.what())));
                }
            } catch (...) {
                if (taskContext) {
                    taskContext->error(QStringLiteral("Unhandled unknown exception in task"));
                }
            }

            if (taskContext) {
                if (succeeded) {
                    if (!Core::Logging::TaskLoggingContext::isTerminalState(taskContext->state())) {
                        Core::Logging::LogManager::instance().finishTask(taskId, QStringLiteral("Completed"));
                    } else {
                        Core::Logging::LogManager::instance().flushTask(taskId);
                    }
                } else {
                    if (!Core::Logging::TaskLoggingContext::isTerminalState(taskContext->state())) {
                        Core::Logging::LogManager::instance().failTask(taskId, QStringLiteral("Task failed with exception"));
                    } else {
                        Core::Logging::LogManager::instance().flushTask(taskId);
                    }
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
            bool succeeded = false;

            try {
                worker(taskContext);
                succeeded = true;
            } catch (const std::exception& ex) {
                if (taskContext) {
                    taskContext->error(QStringLiteral("Unhandled exception in task: %1").arg(QString::fromUtf8(ex.what())));
                }
            } catch (...) {
                if (taskContext) {
                    taskContext->error(QStringLiteral("Unhandled unknown exception in task"));
                }
            }

            if (taskContext) {
                if (succeeded) {
                    if (!Core::Logging::TaskLoggingContext::isTerminalState(taskContext->state())) {
                        Core::Logging::LogManager::instance().finishTask(taskId, QStringLiteral("Completed"));
                    } else {
                        Core::Logging::LogManager::instance().flushTask(taskId);
                    }
                } else {
                    if (!Core::Logging::TaskLoggingContext::isTerminalState(taskContext->state())) {
                        Core::Logging::LogManager::instance().failTask(taskId, QStringLiteral("Task failed with exception"));
                    } else {
                        Core::Logging::LogManager::instance().flushTask(taskId);
                    }
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
