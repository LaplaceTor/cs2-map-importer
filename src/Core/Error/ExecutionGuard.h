#pragma once

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <exception>
#include <type_traits>
#include <utility>

#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Error/Exception.h"
#include "Core/Result/Result.h"

namespace Core::Error {

/**
 * @brief Context descriptor carrying operation stage, resource path, and target path
 *        for diagnostic enrichment when translating exceptions into Core::Result<T>::failure.
 */
struct ExecutionContext {
    QString stage;          // High-level operation stage (e.g. "Locating asset", "Extracting pack entry")
    QString resourcePath;   // Relative or logical asset/resource path (e.g. "materials/models/crate.vmt")
    QString targetPath;     // Physical search/output target path (e.g. "C:/path/to/archive.zip")

    QString formatDetails(const QString& rawDetails = QString()) const;
};

/**
 * @brief Universal Execution Boundary Guard for Core, Domain, and Workflow layers.
 *
 * Guarantees that internal exceptions (Core::Error::Exception, std::exception, or
 * unknown runtime exceptions) do not leak unhandled across component boundaries.
 * Translates exceptions into structured Core::Result<T> outcomes with diagnostic
 * error codes, enriched context details (stage, resource, target), and localized messages.
 */
class ExecutionGuard {
public:
    /**
     * @brief Translates a structured Core::Error::Exception into Result<T>::failure with context enrichment.
     */
    template <typename T = void>
    static Result<T> handleException(
        const Exception& ex,
        const ExecutionContext& ctx,
        const QString& operationSummary = QString())
    {
        Error err = ex.error();
        QString enrichedDetails = ctx.formatDetails(err.details());
        if (enrichedDetails != err.details()) {
            err = Error(err.code(), err.message(), enrichedDetails);
        }
        return Result<T>::failure(
            err,
            operationSummary.isEmpty() ? err.message() : operationSummary);
    }

    template <typename T = void>
    static Result<T> handleException(
        const Exception& ex,
        const QString& operationSummary = QString())
    {
        return handleException<T>(ex, ExecutionContext{}, operationSummary);
    }

    /**
     * @brief Translates a standard std::exception into Result<T>::failure with contextual error classification.
     */
    template <typename T = void>
    static Result<T> handleException(
        const std::exception& ex,
        const ExecutionContext& ctx,
        const QString& operationSummary = QString())
    {
        QString rawWhat = QString::fromUtf8(ex.what());
        ErrorCode code = classifyStdException(ex);
        QString reason = operationSummary.isEmpty()
            ? (ctx.stage.isEmpty() ? rawWhat : QStringLiteral("%1 failed: %2").arg(ctx.stage, rawWhat))
            : operationSummary;
        QString details = ctx.formatDetails(rawWhat);

        return Result<T>::failure(Error(code, reason, details), operationSummary.isEmpty() ? reason : operationSummary);
    }

    template <typename T = void>
    static Result<T> handleException(
        const std::exception& ex,
        const QString& operationSummary = QString())
    {
        return handleException<T>(ex, ExecutionContext{}, operationSummary);
    }

    /**
     * @brief Translates an unknown uncaught exception (catch (...)) into Result<T>::failure.
     */
    template <typename T = void>
    static Result<T> handleUnknownException(
        const ExecutionContext& ctx,
        const QString& operationSummary = QString())
    {
        QString defaultMsg = QCoreApplication::translate("ExecutionGuard", "Unhandled unknown exception");
        QString reason = operationSummary.isEmpty()
            ? (ctx.stage.isEmpty() ? defaultMsg : QStringLiteral("%1 failed: %2").arg(ctx.stage, defaultMsg))
            : operationSummary;
        QString details = ctx.formatDetails();
        return Result<T>::failure(Error(ErrorCode::Unknown, reason, details), operationSummary.isEmpty() ? reason : operationSummary);
    }

    template <typename T = void>
    static Result<T> handleUnknownException(
        const QString& operationSummary = QString())
    {
        return handleUnknownException<T>(ExecutionContext{}, operationSummary);
    }

    /**
     * @brief Executes a callable returning Result<T> inside an exception boundary guard.
     */
    template <typename T, typename WorkerFn>
    static Result<T> guard(
        WorkerFn&& worker,
        const ExecutionContext& ctx,
        const QString& operationSummary = QString())
    {
        try {
            return worker();
        } catch (const Exception& ex) {
            return handleException<T>(ex, ctx, operationSummary);
        } catch (const std::exception& ex) {
            return handleException<T>(ex, ctx, operationSummary);
        } catch (...) {
            return handleUnknownException<T>(ctx, operationSummary);
        }
    }

    template <typename T, typename WorkerFn>
    static Result<T> guard(
        WorkerFn&& worker,
        const QString& operationSummary = QString())
    {
        return guard<T>(std::forward<WorkerFn>(worker), ExecutionContext{}, operationSummary);
    }

    /**
     * @brief Executes a callable with automatic Result payload type deduction and ExecutionContext.
     */
    template <typename WorkerFn>
    static auto guard(
        WorkerFn&& worker,
        const ExecutionContext& ctx,
        const QString& operationSummary = QString())
        -> std::invoke_result_t<WorkerFn>
    {
        using ResultType = std::invoke_result_t<WorkerFn>;
        static_assert(
            Core::is_core_result_v<ResultType>,
            "ExecutionGuard::guard requires WorkerFn to return Core::Result<T>");
        using T = typename ResultType::value_type;
        return guard<T>(std::forward<WorkerFn>(worker), ctx, operationSummary);
    }

    /**
     * @brief Executes a callable with automatic Result payload type deduction.
     */
    template <typename WorkerFn>
    static auto guard(
        WorkerFn&& worker,
        const QString& operationSummary = QString())
        -> std::enable_if_t<!std::is_same_v<std::decay_t<WorkerFn>, ExecutionContext>, std::invoke_result_t<WorkerFn>>
    {
        using ResultType = std::invoke_result_t<WorkerFn>;
        static_assert(
            Core::is_core_result_v<ResultType>,
            "ExecutionGuard::guard requires WorkerFn to return Core::Result<T>");
        using T = typename ResultType::value_type;
        return guard<T>(std::forward<WorkerFn>(worker), ExecutionContext{}, operationSummary);
    }

    /**
     * @brief Classifies a standard C++ exception into a semantic ErrorCode.
     */
    static ErrorCode classifyStdException(const std::exception& ex);
};

} // namespace Core::Error
