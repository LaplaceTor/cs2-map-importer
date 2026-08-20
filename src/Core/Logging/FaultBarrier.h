#pragma once

#include <QMutex>
#include <QString>
#include <QtGlobal>
#include <memory>

namespace Core::Logging {

enum class FaultBarrierState {
    Running,
    FaultDetected,
    Draining,
    Terminated
};

enum class LogSubmissionStatus {
    Accepted,
    RejectedAfterFault,
    RejectedAfterTermination
};

struct LogSubmissionResult {
    LogSubmissionStatus status = LogSubmissionStatus::RejectedAfterTermination;
    quint64 submissionSequence = 0;

    bool accepted() const noexcept { return status == LogSubmissionStatus::Accepted; }
};

struct FaultContext {
    quint64 submissionSequence = 0;
    quint64 boundary = 0;
    quint64 taskId = 0;
    qint64 timestamp = 0;
    QString message;
};

class FaultBarrier {
public:
    FaultBarrier() = default;
    ~FaultBarrier() = default;

    FaultBarrier(const FaultBarrier&) = delete;
    FaultBarrier& operator=(const FaultBarrier&) = delete;
    FaultBarrier(FaultBarrier&&) = delete;
    FaultBarrier& operator=(FaultBarrier&&) = delete;

    FaultBarrierState state() const noexcept;

    LogSubmissionResult submitNormal();
    LogSubmissionResult reportFault(quint64 taskId, qint64 timestamp, const QString& message);

    bool beginDraining() noexcept;
    bool terminate() noexcept;

    FaultContext faultContext() const;
    quint64 acceptedSubmissionCount() const noexcept;

private:
    mutable QMutex m_mutex;
    FaultBarrierState m_state = FaultBarrierState::Running;
    quint64 m_nextSubmissionSequence = 1;
    quint64 m_acceptedSubmissionCount = 0;
    FaultContext m_faultContext;
};

} // namespace Core::Logging
