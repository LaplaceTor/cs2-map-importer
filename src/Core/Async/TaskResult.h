#pragma once

#include <QString>
#include <optional>
#include <type_traits>
#include <utility>

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
 * Represents a single-layer business outcome containing:
 * - status: Success, Failure, Cancelled, Skipped
 * - value / partialValue: The primary business payload of type T
 * - message: Diagnostic or user-facing outcome explanation
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
        return r;
    }

    static TaskResult<T> failure(QString errorMessage, std::optional<T> partialValue = std::nullopt)
    {
        TaskResult<T> r;
        r.m_status = TaskExecutionStatus::Failure;
        r.m_message = std::move(errorMessage);
        r.m_value = std::move(partialValue);
        return r;
    }

    static TaskResult<T> cancelled(QString message = QStringLiteral("Task cancelled"), std::optional<T> value = std::nullopt)
    {
        TaskResult<T> r;
        r.m_status = TaskExecutionStatus::Cancelled;
        r.m_message = std::move(message);
        r.m_value = std::move(value);
        return r;
    }

    static TaskResult<T> skipped(QString reason = QStringLiteral("Task skipped"), std::optional<T> value = std::nullopt)
    {
        TaskResult<T> r;
        r.m_status = TaskExecutionStatus::Skipped;
        r.m_message = std::move(reason);
        r.m_value = std::move(value);
        return r;
    }

    bool isSuccess() const noexcept { return m_status == TaskExecutionStatus::Success; }
    bool isFailure() const noexcept { return m_status == TaskExecutionStatus::Failure; }
    bool isCancelled() const noexcept { return m_status == TaskExecutionStatus::Cancelled; }
    bool isSkipped() const noexcept { return m_status == TaskExecutionStatus::Skipped; }

    TaskExecutionStatus status() const noexcept { return m_status; }
    const QString& message() const noexcept { return m_message; }

    bool hasValue() const noexcept { return m_value.has_value(); }
    const T& value() const { return m_value.value(); }
    T& value() { return m_value.value(); }
    T valueOr(T&& defaultValue) const { return m_value.value_or(std::forward<T>(defaultValue)); }

private:
    TaskExecutionStatus m_status = TaskExecutionStatus::Failure;
    std::optional<T> m_value = std::nullopt;
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
        return r;
    }

    static TaskResult<void> failure(QString errorMessage)
    {
        TaskResult<void> r;
        r.m_status = TaskExecutionStatus::Failure;
        r.m_message = std::move(errorMessage);
        return r;
    }

    static TaskResult<void> cancelled(QString message = QStringLiteral("Task cancelled"))
    {
        TaskResult<void> r;
        r.m_status = TaskExecutionStatus::Cancelled;
        r.m_message = std::move(message);
        return r;
    }

    static TaskResult<void> skipped(QString reason = QStringLiteral("Task skipped"))
    {
        TaskResult<void> r;
        r.m_status = TaskExecutionStatus::Skipped;
        r.m_message = std::move(reason);
        return r;
    }

    bool isSuccess() const noexcept { return m_status == TaskExecutionStatus::Success; }
    bool isFailure() const noexcept { return m_status == TaskExecutionStatus::Failure; }
    bool isCancelled() const noexcept { return m_status == TaskExecutionStatus::Cancelled; }
    bool isSkipped() const noexcept { return m_status == TaskExecutionStatus::Skipped; }

    TaskExecutionStatus status() const noexcept { return m_status; }
    const QString& message() const noexcept { return m_message; }

private:
    TaskExecutionStatus m_status = TaskExecutionStatus::Failure;
    QString m_message;
};

} // namespace Core::Async
