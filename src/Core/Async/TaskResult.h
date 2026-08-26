#pragma once

#include <QString>
#include <optional>
#include <type_traits>
#include <utility>

#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"

namespace Core::Async {

/**
 * @brief Business outcome status for operations in Workflow, Application, and Domain layers.
 *
 * Architectural Role:
 * - TaskExecutionStatus represents the **Business Outcome Plane** (业务操作结果).
 * - Core::Logging::TaskState represents the **Execution Lifecycle Plane** (任务执行生命周期).
 *
 * AsyncTaskRunner bridges the two: evaluating the business outcome (along with logged errors
 * and exception safety) to transition the underlying TaskState in LogManager.
 */
enum class TaskExecutionStatus {
    Success,
    Failure,
    Cancelled,
    Skipped
};

/**
 * @brief Standardized result wrapper for business operations in Workflow and Application services.
 *
 * ### Result Semantic Contract:
 * - **`status()` / `isSuccess()` / `isFailure()` / `isCancelled()` / `isSkipped()`**:
 *     Authoritative source for business outcome branching. Always check `status()` or `isSuccess()` / `isFailure()`
 *     rather than inspecting `errorCode()` alone.
 * - **`value()`**: Strong-typed business payload (only valid on `isSuccess()`, or partial on failure).
 * - **`error()` / `errorCode()`**: Machine-interpretable structured diagnostic error (`Core::Error::Error`).
 *     - Only carries non-success failure/cancellation semantics when `isFailure()` or `isCancelled()`.
 *     - For `Skipped` results, `error()` is `Error::success()` (as skipping is a benign non-fault path),
 *       and the specific reason is stored in `message()`.
 *     - `error().code()`: Standardized `Core::Error::ErrorCode` enum for programmatic error routing.
 *     - `error().message()`: Low-level domain/system failure reason.
 *     - `error().details()`: Technical details (file paths, stderr, syntax error lines, etc.).
 * - **`message()`**: High-level operation summary for presentation / UI (e.g. "Validation failed for CS2", "Skipped: up to date").
 *     - For `Failure`: falls back to `error().message()` if no custom operation summary was specified.
 *     - For `Skipped` / `Cancelled` / `Success`: carries the respective status explanation.
 * - **`details()`**: Direct proxy to `error().details()` for technical diagnostic context.
 *
 * @tparam T The business payload type (or void).
 */
template <typename T = void>
class TaskResult {
public:
    TaskResult() = default;

    /**
     * @brief Constructs a successful result with payload and optional status note.
     */
    static TaskResult<T> success(T value, QString message = QString())
    {
        TaskResult<T> r;
        r.m_status = TaskExecutionStatus::Success;
        r.m_value = std::move(value);
        r.m_message = std::move(message);
        r.m_error = Core::Error::Error::success();
        return r;
    }

    /**
     * @brief Constructs a failure result with structured Error and optional high-level operation summary.
     */
    static TaskResult<T> failure(Core::Error::Error error, QString operationSummary = QString(), std::optional<T> partialValue = std::nullopt)
    {
        TaskResult<T> r;
        r.m_status = TaskExecutionStatus::Failure;
        r.m_message = operationSummary.isEmpty() ? error.message() : std::move(operationSummary);
        r.m_error = std::move(error);
        r.m_value = std::move(partialValue);
        return r;
    }

    /**
     * @brief Constructs a failure result with partial value and structured Error.
     */
    static TaskResult<T> failure(Core::Error::Error error, std::optional<T> partialValue)
    {
        return failure(std::move(error), QString(), std::move(partialValue));
    }

    /**
     * @brief Constructs a failure result with ErrorCode, error message, and optional details.
     */
    static TaskResult<T> failure(Core::Error::ErrorCode code, QString errorMessage = QString(), QString details = QString(), std::optional<T> partialValue = std::nullopt)
    {
        return failure(Core::Error::Error(code, std::move(errorMessage), std::move(details)), QString(), std::move(partialValue));
    }

    /**
     * @brief Compatibility overload constructing a failure result with default ErrorCode::OperationFailed.
     */
    static TaskResult<T> failure(QString errorMessage, std::optional<T> partialValue = std::nullopt)
    {
        return failure(Core::Error::ErrorCode::OperationFailed, std::move(errorMessage), QString(), std::move(partialValue));
    }

    /**
     * @brief Constructs a cancelled result carrying cancellation reason.
     */
    static TaskResult<T> cancelled(QString reason = QStringLiteral("Task cancelled"), std::optional<T> value = std::nullopt)
    {
        TaskResult<T> r;
        r.m_status = TaskExecutionStatus::Cancelled;
        r.m_message = reason;
        r.m_error = Core::Error::Error(Core::Error::ErrorCode::Cancelled, std::move(reason));
        r.m_value = std::move(value);
        return r;
    }

    /**
     * @brief Constructs a skipped result carrying skip reason.
     */
    static TaskResult<T> skipped(QString reason = QStringLiteral("Task skipped"), std::optional<T> value = std::nullopt)
    {
        TaskResult<T> r;
        r.m_status = TaskExecutionStatus::Skipped;
        r.m_message = std::move(reason);
        r.m_error = Core::Error::Error::success();
        r.m_value = std::move(value);
        return r;
    }

    // Status queries
    bool isSuccess() const noexcept { return m_status == TaskExecutionStatus::Success; }
    bool isFailure() const noexcept { return m_status == TaskExecutionStatus::Failure; }
    bool isCancelled() const noexcept { return m_status == TaskExecutionStatus::Cancelled; }
    bool isSkipped() const noexcept { return m_status == TaskExecutionStatus::Skipped; }

    TaskExecutionStatus status() const noexcept { return m_status; }

    // Error & Diagnostics Contract
    const Core::Error::Error& error() const noexcept { return m_error; }
    Core::Error::ErrorCode errorCode() const noexcept { return m_error.code(); }

    /// @brief High-level operation summary (for user presentation / logging summary).
    const QString& message() const noexcept { return m_message.isEmpty() ? m_error.message() : m_message; }

    /// @brief Technical diagnostic details (stderr, file paths, AST details).
    const QString& details() const noexcept { return m_error.details(); }

    // Payload Access
    bool hasValue() const noexcept { return m_value.has_value(); }
    bool has_value() const noexcept { return m_value.has_value(); }
    const T& value() const { return m_value.value(); }
    T& value() { return m_value.value(); }
    const T* operator->() const { return &m_value.value(); }
    T* operator->() { return &m_value.value(); }
    const T& operator*() const { return m_value.value(); }
    T& operator*() { return m_value.value(); }
    T valueOr(T defaultValue) const { return m_value.value_or(std::move(defaultValue)); }

private:
    TaskExecutionStatus m_status = TaskExecutionStatus::Failure;
    std::optional<T> m_value = std::nullopt;
    Core::Error::Error m_error = Core::Error::Error(Core::Error::ErrorCode::Unknown);
    QString m_message;
};

/**
 * @brief Specialization of TaskResult for void business payload.
 */
template <>
class TaskResult<void> {
public:
    TaskResult() = default;

    static TaskResult<void> success(QString message = QString())
    {
        TaskResult<void> r;
        r.m_status = TaskExecutionStatus::Success;
        r.m_message = std::move(message);
        r.m_error = Core::Error::Error::success();
        return r;
    }

    static TaskResult<void> failure(Core::Error::Error error, QString operationSummary = QString())
    {
        TaskResult<void> r;
        r.m_status = TaskExecutionStatus::Failure;
        r.m_message = operationSummary.isEmpty() ? error.message() : std::move(operationSummary);
        r.m_error = std::move(error);
        return r;
    }

    static TaskResult<void> failure(Core::Error::ErrorCode code, QString errorMessage = QString(), QString details = QString())
    {
        return failure(Core::Error::Error(code, std::move(errorMessage), std::move(details)));
    }

    static TaskResult<void> failure(QString errorMessage)
    {
        return failure(Core::Error::ErrorCode::OperationFailed, std::move(errorMessage));
    }

    static TaskResult<void> cancelled(QString reason = QStringLiteral("Task cancelled"))
    {
        TaskResult<void> r;
        r.m_status = TaskExecutionStatus::Cancelled;
        r.m_message = reason;
        r.m_error = Core::Error::Error(Core::Error::ErrorCode::Cancelled, std::move(reason));
        return r;
    }

    static TaskResult<void> skipped(QString reason = QStringLiteral("Task skipped"))
    {
        TaskResult<void> r;
        r.m_status = TaskExecutionStatus::Skipped;
        r.m_message = std::move(reason);
        r.m_error = Core::Error::Error::success();
        return r;
    }

    bool isSuccess() const noexcept { return m_status == TaskExecutionStatus::Success; }
    bool isFailure() const noexcept { return m_status == TaskExecutionStatus::Failure; }
    bool isCancelled() const noexcept { return m_status == TaskExecutionStatus::Cancelled; }
    bool isSkipped() const noexcept { return m_status == TaskExecutionStatus::Skipped; }

    TaskExecutionStatus status() const noexcept { return m_status; }
    const Core::Error::Error& error() const noexcept { return m_error; }
    Core::Error::ErrorCode errorCode() const noexcept { return m_error.code(); }
    const QString& message() const noexcept { return m_message.isEmpty() ? m_error.message() : m_message; }
    const QString& details() const noexcept { return m_error.details(); }

private:
    TaskExecutionStatus m_status = TaskExecutionStatus::Failure;
    Core::Error::Error m_error = Core::Error::Error(Core::Error::ErrorCode::Unknown);
    QString m_message;
};

} // namespace Core::Async
