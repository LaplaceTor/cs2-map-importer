#pragma once

#include <QCoreApplication>
#include "Core/Async/CancellationToken.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Result/Result.h"
#include <QString>
#include <functional>
#include <type_traits>

namespace Workflow::Common {

/**
 * @brief Unified execution context for import workflows.
 *
 * Bundles Core::Logging::TaskLoggingContext and Core::Async::CancellationToken
 * into a single cohesive context object to provide:
 * - Unified cooperative cancellation checking across all workflow stages.
 * - Direct forwarding to task logging and progress reporting.
 * - Elimination of dual-parameter passing (token + loggingCtx).
 */
class ImportContext {
public:
    explicit ImportContext(
        Core::Logging::TaskLoggingContext* loggingCtx = nullptr,
        Core::Async::CancellationToken token = Core::Async::CancellationToken{})
        : m_loggingCtx(loggingCtx)
        , m_token(std::move(token))
    {
    }

    // Cancellation API
    bool isCancelled() const noexcept {
        return m_token.isCancelled();
    }

    void cancel() noexcept {
        m_token.cancel();
    }

    const Core::Async::CancellationToken& token() const noexcept {
        return m_token;
    }

    /**
     * @brief Checks cancellation status and returns a cancelled Result<void> if cancelled.
     */
    Core::Result<void> checkCancelled(
        const QString& message = QCoreApplication::translate("ImportContext", "Operation was cancelled")) const
    {
        if (isCancelled()) {
            return Core::Result<void>::cancelled(message);
        }
        return Core::Result<void>::success();
    }

    // Logging & Progress API forwarding to TaskLoggingContext
    Core::Logging::TaskLoggingContext* loggingContext() const noexcept {
        return m_loggingCtx;
    }

    void info(const QString& message) const {
        if (m_loggingCtx) {
            m_loggingCtx->info(message);
            m_loggingCtx->flush();
        }
    }

    void warning(const QString& message) const {
        if (m_loggingCtx) {
            m_loggingCtx->warning(message);
            m_loggingCtx->flush();
        }
    }

    void error(const QString& message) const {
        if (m_loggingCtx) {
            m_loggingCtx->error(message);
            m_loggingCtx->flush();
        }
    }

    void updateProgress(double progress, const QString& message = QString()) const {
        if (m_loggingCtx) {
            m_loggingCtx->updateProgress(progress, message);
            m_loggingCtx->flush();
        }
    }

    quint64 taskId() const noexcept {
        return m_loggingCtx ? m_loggingCtx->taskId() : 0;
    }

    /**
     * @brief Executes a pipeline step with automatic cancellation checks before and after execution.
     */
    template <typename StepFn>
    auto runStep(const QString& stepName, double progress, StepFn&& fn) const
        -> std::invoke_result_t<StepFn>
    {
        using ReturnType = std::invoke_result_t<StepFn>;
        if (isCancelled()) {
            return ReturnType::cancelled(QCoreApplication::translate("ImportContext", "Cancelled before step: %1").arg(stepName));
        }

        if (progress >= 0.0) {
            updateProgress(progress, stepName);
        }

        auto result = fn();

        if (isCancelled() && !result.isCancelled()) {
            return ReturnType::cancelled(QCoreApplication::translate("ImportContext", "Cancelled during step: %1").arg(stepName));
        }

        return result;
    }

private:
    Core::Logging::TaskLoggingContext* m_loggingCtx = nullptr;
    Core::Async::CancellationToken m_token;
};

} // namespace Workflow::Common
