#pragma once

#include "Core/Async/CancellationToken.h"
#include "Core/Logging/LogManager.h"
#include <QtGlobal>
#include <QString>

namespace Application::Async {

/**
 * @brief Thread-safe execution handle for managing an asynchronous task.
 *
 * Returned by AsyncTaskRunner and Application services to the UI layer.
 * Enables UI components (e.g. MainController) to query task status and
 * initiate cooperative cancellation without managing raw tokens or threads.
 */
class [[nodiscard]] TaskHandle {
public:
    TaskHandle() = default;
    TaskHandle(quint64 taskId, Core::Async::CancellationToken token)
        : m_taskId(taskId)
        , m_token(std::move(token))
        , m_valid(true)
    {
    }

    quint64 taskId() const noexcept {
        return m_taskId;
    }

    bool isValid() const noexcept {
        return m_valid;
    }

    bool isCancelled() const noexcept {
        return m_token.isCancelled();
    }

    void cancel(const QString& message = QString()) noexcept {
        if (m_valid) {
            m_token.cancel();
            if (m_taskId != 0) {
                Core::Logging::LogManager::instance().cancelTask(m_taskId, message);
            }
        }
    }

private:
    quint64 m_taskId = 0;
    Core::Async::CancellationToken m_token;
    bool m_valid = false;
};

} // namespace Application::Async
