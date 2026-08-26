#pragma once

#include <QString>
#include <optional>
#include <type_traits>
#include <utility>

namespace Application::Async {

/**
 * @brief Standard execution status outcome for Workflow and Application tasks.
 */
enum class TaskExecutionStatus {
    Success,
    Failure,
    Cancelled,
    Skipped
};

/**
 * @brief Standardized result wrapper for asynchronous and workflow operations.
 * Explicitly distinguishes between Success, Failure, Cancelled, and Skipped states.
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

    static TaskResult<T> cancelled(QString message = QStringLiteral("Task cancelled"))
    {
        TaskResult<T> r;
        r.m_status = TaskExecutionStatus::Cancelled;
        r.m_message = std::move(message);
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
    T valueOr(T defaultVal) const { return m_value.value_or(std::move(defaultVal)); }

    const std::optional<T>& optionalValue() const noexcept { return m_value; }

    explicit operator bool() const noexcept { return isSuccess(); }

private:
    TaskExecutionStatus m_status = TaskExecutionStatus::Failure;
    std::optional<T> m_value = std::nullopt;
    QString m_message;
};

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

    explicit operator bool() const noexcept { return isSuccess(); }

private:
    TaskExecutionStatus m_status = TaskExecutionStatus::Failure;
    QString m_message;
};

} // namespace Application::Async

