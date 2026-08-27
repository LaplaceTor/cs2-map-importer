#pragma once

#include <QString>
#include <exception>
#include <type_traits>
#include <utility>
#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Error/Exception.h"
#include "Core/Result/Result.h"

namespace Application::Execution {

/**
 * @brief Universal Application Execution Boundary Guard.
 *
 * Enforces the architectural contract that no internal exceptions (Core::Error::Exception,
 * std::exception, or unknown runtime exceptions) escape across the Application API boundary.
 *
 * Translates exceptions into structured Core::Result<T>::failure outcomes using a single,
 * consistent translation model shared between synchronous APIs and AsyncTaskRunner.
 */
class ExecutionGuard {
public:
    /**
     * @brief Translates a structured Core::Error::Exception into Result<T>::failure.
     */
    template <typename T = void>
    static Core::Result<T> handleException(
        const Core::Error::Exception& ex,
        const QString& operationSummary = QString())
    {
        return Core::Result<T>::failure(
            ex.error(),
            operationSummary.isEmpty() ? ex.error().message() : operationSummary);
    }

    /**
     * @brief Translates a standard std::exception into Result<T>::failure.
     */
    template <typename T = void>
    static Core::Result<T> handleException(
        const std::exception& ex,
        const QString& operationSummary = QString())
    {
        return Core::Result<T>::failure(
            Core::Error::Error::unknown(
                QStringLiteral("Unhandled standard exception"),
                QString::fromUtf8(ex.what())),
            operationSummary.isEmpty() ? QString::fromUtf8(ex.what()) : operationSummary);
    }

    /**
     * @brief Translates an unknown uncaught exception (catch (...)) into Result<T>::failure.
     */
    template <typename T = void>
    static Core::Result<T> handleUnknownException(
        const QString& operationSummary = QString())
    {
        return Core::Result<T>::failure(
            Core::Error::Error::unknown(QStringLiteral("Unhandled unknown exception")),
            operationSummary.isEmpty() ? QStringLiteral("Unhandled unknown exception") : operationSummary);
    }

    /**
     * @brief Executes a callable returning Core::Result<T> inside an exception boundary guard.
     *
     * @tparam T The payload type of the Result<T>.
     * @param worker A callable returning Core::Result<T>.
     * @param operationSummary Optional high-level operation summary message on failure.
     * @return Core::Result<T> The worker result, or a structured failure if an exception was thrown.
     */
    template <typename T, typename WorkerFn>
    static Core::Result<T> guard(
        WorkerFn&& worker,
        const QString& operationSummary = QString())
    {
        static_assert(
            std::is_invocable_r_v<Core::Result<T>, WorkerFn>,
            "ExecutionGuard::guard<T> requires WorkerFn to be callable and return Core::Result<T>");
        try {
            return worker();
        } catch (const Core::Error::Exception& ex) {
            return handleException<T>(ex, operationSummary);
        } catch (const std::exception& ex) {
            return handleException<T>(ex, operationSummary);
        } catch (...) {
            return handleUnknownException<T>(operationSummary);
        }
    }

    /**
     * @brief Executes a callable returning Core::Result<T> with automatic payload type deduction.
     */
    template <typename WorkerFn>
    static auto guard(
        WorkerFn&& worker,
        const QString& operationSummary = QString())
        -> std::invoke_result_t<WorkerFn>
    {
        using ReturnType = std::invoke_result_t<WorkerFn>;
        static_assert(
            Core::is_core_result_v<ReturnType>,
            "ExecutionGuard::guard requires WorkerFn to return Core::Result<T>");
        using ValueType = typename ReturnType::value_type;
        return guard<ValueType>(std::forward<WorkerFn>(worker), operationSummary);
    }
};

} // namespace Application::Execution

