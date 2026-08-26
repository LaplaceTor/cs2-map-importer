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
 * Distinctly separates:
 * - status: Success, Failure, Cancelled, Skipped
 * - value: The primary business payload of type T (or partialValue)
 * - error: Structured Core::Error::Error (carrying ErrorCode, message, details for Failure/Cancelled)
 * - message: Informational status explanation (success note, skip reason, cancel reason, or failure summary)
 *
 * @tparam T The business payload type (or void).
 */
template <typename T = void>
class TaskResult {
public:
    TaskResult() = default;

    static TaskResult<T> success(T value, QString message = QString())
    {
        TaskResult<T> r;
        r.m_status = TaskExecutionStatus::Success;
        r.m_value = std::move(value);
        r.m_message = std::move(message);
        r.m_error = Core::Error::Error::success();
        return r;
    }

    static TaskResult<T> failure(Core::Error::Error error, std::optional<T> partialValue = std::nullopt)
    {
        TaskResult<T> r;
        r.m_status = TaskExecutionStatus::Failure;
        r.m_message = error.message();
        r.m_error = std::move(error);
        r.m_value = std::move(partialValue);
        return r;
    }

    static TaskResult<T> failure(Core::Error::ErrorCode code, QString errorMessage = QString(), std::optional<T> partialValue = std::nullopt)
    {
        return failure(Core::Error::Error(code, std::move(errorMessage)), std::move(partialValue));
    }

    static TaskResult<T> failure(QString errorMessage, std::optional<T> partialValue = std::nullopt)
    {
        return failure(Core::Error::ErrorCode::OperationFailed, std::move(errorMessage), std::move(partialValue));
    }

    static TaskResult<T> cancelled(QString reason = QStringLiteral("Task cancelled"), std::optional<T> value = std::nullopt)
    {
        TaskResult<T> r;
        r.m_status = TaskExecutionStatus::Cancelled;
        r.m_message = reason;
        r.m_error = Core::Error::Error(Core::Error::ErrorCode::Cancelled, std::move(reason));
        r.m_value = std::move(value);
        return r;
    }

    static TaskResult<T> skipped(QString reason = QStringLiteral("Task skipped"), std::optional<T> value = std::nullopt)
    {
        TaskResult<T> r;
        r.m_status = TaskExecutionStatus::Skipped;
        r.m_message = std::move(reason);
        r.m_error = Core::Error::Error::success();
        r.m_value = std::move(value);
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

    static TaskResult<void> failure(Core::Error::Error error)
    {
        TaskResult<void> r;
        r.m_status = TaskExecutionStatus::Failure;
        r.m_message = error.message();
        r.m_error = std::move(error);
        return r;
    }

    static TaskResult<void> failure(Core::Error::ErrorCode code, QString errorMessage = QString())
    {
        return failure(Core::Error::Error(code, std::move(errorMessage)));
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
