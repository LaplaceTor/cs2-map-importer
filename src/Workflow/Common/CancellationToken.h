#pragma once

#include <atomic>
#include <memory>

namespace Workflow::Common {

/**
 * @brief Cooperative cancellation token for workflow use cases.
 *
 * Tokens are explicitly passed as parameters through the call chain and share
 * state through an atomic flag; no global flags are used. Copying a token
 * shares the same cancellation state, so owners can hand out cancellable views
 * of a single cancellation source.
 */
class CancellationToken {
public:
    CancellationToken()
        : m_state(std::make_shared<std::atomic_bool>(false)) {}

    void cancel() noexcept {
        m_state->store(true, std::memory_order_relaxed);
    }

    bool isCancelled() const noexcept {
        return m_state->load(std::memory_order_relaxed);
    }

private:
    std::shared_ptr<std::atomic_bool> m_state;
};

} // namespace Workflow::Common
