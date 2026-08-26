#pragma once

#include <QObject>
#include <QPointer>
#include <QThreadPool>
#include <QMetaObject>
#include <QString>
#include <exception>
#include <functional>
#include <memory>
#include <utility>

#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskLoggingContext.h"

namespace Application::Async {

/**
 * @brief Helper for standardized asynchronous task execution with TaskLoggingContext,
 * multi-level hierarchical sub-tasks, exception safety, and Qt thread-safe callback delivery.
 */
class AsyncTaskRunner {
public:
    /**
     * @brief Runs an async worker with a return value and delivers the result to callback on context thread.
     */
    template <typename ResultType, typename WorkerFn, typename CallbackFn>
    static void run(
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback,
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

            if (!contextGuard) {
                return;
            }

            QMetaObject::invokeMethod(contextGuard.data(), [contextGuard, callback = std::move(callback), res = std::move(result)]() {
                if (contextGuard) {
                    callback(res);
                }
            }, Qt::QueuedConnection);
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
    template <typename WorkerFn, typename CallbackFn>
    static void runVoid(
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback,
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

            if (!contextGuard) {
                return;
            }

            QMetaObject::invokeMethod(contextGuard.data(), [contextGuard, callback = std::move(callback)]() {
                if (contextGuard) {
                    callback();
                }
            }, Qt::QueuedConnection);
        };

        if (pool) {
            pool->start(std::move(workerLambda));
        } else {
            QThreadPool::globalInstance()->start(std::move(workerLambda));
        }
    }

    /**
     * @brief Convenience helper to run a child sub-task attached to parentTaskId.
     */
    template <typename ResultType, typename WorkerFn, typename CallbackFn>
    static void runChild(
        quint64 parentTaskId,
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback,
        QThreadPool* pool = QThreadPool::globalInstance())
    {
        run<ResultType>(taskName, context, std::forward<WorkerFn>(worker), std::forward<CallbackFn>(callback), pool, parentTaskId);
    }

    /**
     * @brief Convenience helper to run a child void sub-task attached to parentTaskId.
     */
    template <typename WorkerFn, typename CallbackFn>
    static void runChildVoid(
        quint64 parentTaskId,
        const QString& taskName,
        QObject* context,
        WorkerFn&& worker,
        CallbackFn&& callback,
        QThreadPool* pool = QThreadPool::globalInstance())
    {
        runVoid(taskName, context, std::forward<WorkerFn>(worker), std::forward<CallbackFn>(callback), pool, parentTaskId);
    }
};

} // namespace Application::Async
