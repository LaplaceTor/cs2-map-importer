#include "FaultBarrier.h"

#include <QMutexLocker>

namespace Core::Logging {

FaultBarrierState FaultBarrier::state() const noexcept
{
    QMutexLocker locker(&m_mutex);
    return m_state;
}

LogSubmissionResult FaultBarrier::submitNormal()
{
    QMutexLocker locker(&m_mutex);
    if (m_state == FaultBarrierState::Terminated) {
        return {LogSubmissionStatus::RejectedAfterTermination, 0};
    }
    if (m_state != FaultBarrierState::Running) {
        return {LogSubmissionStatus::RejectedAfterFault, 0};
    }

    const quint64 sequence = m_nextSubmissionSequence++;
    ++m_acceptedSubmissionCount;
    return {LogSubmissionStatus::Accepted, sequence};
}

LogSubmissionResult FaultBarrier::reportFault(quint64 taskId, qint64 timestamp, const QString& message)
{
    QMutexLocker locker(&m_mutex);
    if (m_state == FaultBarrierState::Terminated) {
        return {LogSubmissionStatus::RejectedAfterTermination, 0};
    }
    if (m_state != FaultBarrierState::Running) {
        return {LogSubmissionStatus::RejectedAfterFault, 0};
    }

    const quint64 sequence = m_nextSubmissionSequence++;
    m_faultContext.submissionSequence = sequence;
    m_faultContext.boundary = sequence - 1;
    m_faultContext.taskId = taskId;
    m_faultContext.timestamp = timestamp;
    m_faultContext.message = message;
    m_state = FaultBarrierState::FaultDetected;
    return {LogSubmissionStatus::Accepted, sequence};
}

bool FaultBarrier::beginDraining() noexcept
{
    QMutexLocker locker(&m_mutex);
    if (m_state != FaultBarrierState::FaultDetected) {
        return false;
    }
    m_state = FaultBarrierState::Draining;
    return true;
}

bool FaultBarrier::terminate() noexcept
{
    QMutexLocker locker(&m_mutex);
    if (m_state != FaultBarrierState::Draining) {
        return false;
    }
    m_state = FaultBarrierState::Terminated;
    return true;
}

FaultContext FaultBarrier::faultContext() const
{
    QMutexLocker locker(&m_mutex);
    return m_faultContext;
}

quint64 FaultBarrier::acceptedSubmissionCount() const noexcept
{
    QMutexLocker locker(&m_mutex);
    return m_acceptedSubmissionCount;
}

} // namespace Core::Logging
