#pragma once

#include <QString>
#include <optional>
#include <type_traits>
#include <utility>

#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"

namespace Core {

/**
 * @brief Universal operation outcome status for Domain, Workflow, and Application layers.
 *
 * Architectural Role:
 * - ResultStatus represents the **Business Outcome Plane** (业务操作结果).
 * - Core::Logging::TaskState represents the **Execution Lifecycle Plane** (任务执行生命周期).
 *
 * AsyncTaskRunner bridges the two: evaluating the business outcome (along with logged errors
 * and exception safety) to transition the underlying TaskState in LogManager.
 */
enum class ResultStatus {
    Success,
    Failure,
    Cancelled,
    Skipped
};

/**
 * @brief Standardized result wrapper for business operations across all layers.
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
class Result {
public:
    using value_type = T;

    Result() = default;

    /**
     * @brief Constructs a successful result with payload and optional status note.
     */
    static Result<T> success(T value, QString message = QString())
    {
        Result<T> r;
        r.m_status = ResultStatus::Success;
        r.m_value = std::move(value);
        r.m_message = std::move(message);
        r.m_error = Core::Error::Error::success();
        return r;
    }

    /**
     * @brief Constructs a failed result with structured error, optional operation summary, and optional partial value.
     */
    static Result<T> failure(Core::Error::Error error, QString message = QString(), std::optional<T> partialValue = std::nullopt)
    {
        Result<T> r;
        r.m_status = ResultStatus::Failure;
        if (error.isSuccess()) {
            r.m_error = Core::Error::Error(Core::Error::ErrorCode::OperationFailed, message);
        } else {
            r.m_error = std::move(error);
        }
        r.m_message = std::move(message);
        r.m_value = std::move(partialValue);
        return r;
    }

    template <typename U = T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, QString>>>
    static Result<T> failure(Core::Error::Error error, std::optional<T> partialValue)
    {
        return failure(std::move(error), QString(), std::move(partialValue));
    }

    static Result<T> failure(Core::Error::ErrorCode code, const QString& failureReason, const QString& details = QString(), std::optional<T> partialValue = std::nullopt)
    {
        return failure(Core::Error::Error(code, failureReason, details), QString(), std::move(partialValue));
    }

    template <typename U = T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, QString>>>
    static Result<T> failure(Core::Error::ErrorCode code, const QString& failureReason, std::optional<T> partialValue)
    {
        return failure(code, failureReason, QString(), std::move(partialValue));
    }

    /**
     * @brief Constructs a cancelled result with reason note and optional partial value.
     */
    static Result<T> cancelled(QString message = QStringLiteral("Operation cancelled"), std::optional<T> partialValue = std::nullopt)
    {
        Result<T> r;
        r.m_status = ResultStatus::Cancelled;
        r.m_message = std::move(message);
        r.m_error = Core::Error::Error(Core::Error::ErrorCode::Cancelled, r.m_message);
        r.m_value = std::move(partialValue);
        return r;
    }

    /**
     * @brief Constructs a skipped result with explanation and optional existing value.
     */
    static Result<T> skipped(QString message = QStringLiteral("Operation skipped"), std::optional<T> existingValue = std::nullopt)
    {
        Result<T> r;
        r.m_status = ResultStatus::Skipped;
        r.m_message = std::move(message);
        r.m_error = Core::Error::Error::success();
        r.m_value = std::move(existingValue);
        return r;
    }

    bool isSuccess() const noexcept { return m_status == ResultStatus::Success; }
    bool isFailure() const noexcept { return m_status == ResultStatus::Failure; }
    bool isCancelled() const noexcept { return m_status == ResultStatus::Cancelled; }
    bool isSkipped() const noexcept { return m_status == ResultStatus::Skipped; }

    ResultStatus status() const noexcept { return m_status; }

    const Core::Error::Error& error() const noexcept { return m_error; }
    Core::Error::ErrorCode errorCode() const noexcept { return m_error.code(); }

    const QString& message() const noexcept { return m_message.isEmpty() ? m_error.message() : m_message; }
    const QString& details() const noexcept { return m_error.details(); }

    bool hasValue() const noexcept { return m_value.has_value(); }
    bool has_value() const noexcept { return m_value.has_value(); }
    const T& value() const { return m_value.value(); }
    T& value() { return m_value.value(); }

    const T* operator->() const { return &m_value.value(); }
    T* operator->() { return &m_value.value(); }

    const T& operator*() const { return m_value.value(); }
    T& operator*() { return m_value.value(); }

    template <typename U>
    T valueOr(U&& defaultValue) const
    {
        return m_value.value_or(std::forward<U>(defaultValue));
    }

private:
    ResultStatus m_status = ResultStatus::Failure;
    std::optional<T> m_value;
    Core::Error::Error m_error = Core::Error::Error(Core::Error::ErrorCode::Unknown);
    QString m_message;
};

/**
 * @brief Specialization of Result for operations producing no business payload (void).
 */
template <>
class Result<void> {
public:
    using value_type = void;

    Result() = default;

    /**
     * @brief Constructs a successful void result with optional completion note.
     */
    static Result<void> success(QString message = QString())
    {
        Result<void> r;
        r.m_status = ResultStatus::Success;
        r.m_message = std::move(message);
        r.m_error = Core::Error::Error::success();
        return r;
    }

    /**
     * @brief Constructs a failed void result with structured error and optional operation summary.
     */
    static Result<void> failure(Core::Error::Error error, QString message = QString())
    {
        Result<void> r;
        r.m_status = ResultStatus::Failure;
        if (error.isSuccess()) {
            r.m_error = Core::Error::Error(Core::Error::ErrorCode::OperationFailed, message);
        } else {
            r.m_error = std::move(error);
        }
        r.m_message = std::move(message);
        return r;
    }

    /**
     * @brief Convenience overload constructing a failed void result with ErrorCode, specific reason, and optional details.
     */
    static Result<void> failure(Core::Error::ErrorCode code, const QString& failureReason, const QString& details = QString())
    {
        return failure(Core::Error::Error(code, failureReason, details), QString());
    }

    /**
     * @brief Constructs a cancelled void result with reason note.
     */
    static Result<void> cancelled(QString message = QStringLiteral("Operation cancelled"))
    {
        Result<void> r;
        r.m_status = ResultStatus::Cancelled;
        r.m_message = std::move(message);
        r.m_error = Core::Error::Error(Core::Error::ErrorCode::Cancelled, r.m_message);
        return r;
    }

    /**
     * @brief Constructs a skipped void result with explanation.
     */
    static Result<void> skipped(QString message = QStringLiteral("Operation skipped"))
    {
        Result<void> r;
        r.m_status = ResultStatus::Skipped;
        r.m_message = std::move(message);
        r.m_error = Core::Error::Error::success();
        return r;
    }

    bool isSuccess() const noexcept { return m_status == ResultStatus::Success; }
    bool isFailure() const noexcept { return m_status == ResultStatus::Failure; }
    bool isCancelled() const noexcept { return m_status == ResultStatus::Cancelled; }
    bool isSkipped() const noexcept { return m_status == ResultStatus::Skipped; }

    ResultStatus status() const noexcept { return m_status; }
    const Core::Error::Error& error() const noexcept { return m_error; }
    Core::Error::ErrorCode errorCode() const noexcept { return m_error.code(); }
    const QString& message() const noexcept { return m_message.isEmpty() ? m_error.message() : m_message; }
    const QString& details() const noexcept { return m_error.details(); }

private:
    ResultStatus m_status = ResultStatus::Failure;
    Core::Error::Error m_error = Core::Error::Error(Core::Error::ErrorCode::Unknown);
    QString m_message;
};

} // namespace Core
