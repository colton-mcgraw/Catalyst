#include <catalyst/core/subscription.hpp>

#include <catalyst/core/dispatcher.hpp>

#include <utility>

namespace catalyst::core
{
    subscription::subscription(std::weak_ptr<detail::dispatcher_state> state,
                               event_type_id type_id,
                               std::size_t token,
                               std::shared_ptr<std::atomic_bool> active) noexcept
        : m_state(std::move(state)),
          m_type_id(type_id),
          m_token(token),
          m_active(std::move(active))
    {
    }

    subscription::subscription(subscription &&other) noexcept
        : m_state(std::move(other.m_state)),
          m_type_id(std::exchange(other.m_type_id, event_base::invalid_type_id())),
          m_token(std::exchange(other.m_token, 0u)),
          m_active(std::move(other.m_active))
    {
        other.m_state.reset();
    }

    subscription &subscription::operator=(subscription &&other) noexcept
    {
        if (this == &other)
            return *this;

        reset();

        m_state = std::move(other.m_state);
        m_type_id = std::exchange(other.m_type_id, event_base::invalid_type_id());
        m_token = std::exchange(other.m_token, 0u);
        m_active = std::move(other.m_active);
        other.m_state.reset();

        return *this;
    }

    subscription::~subscription()
    {
        reset();
    }

    void subscription::reset() noexcept
    {
        // Deactivate first: even if the dispatcher's table is not touched below, the handler will never run again.
        if (m_active)
            m_active->store(false, std::memory_order_relaxed);

        if (m_token != 0u)
        {
            // The dispatcher may already be gone; in that case there is nothing to remove.
            if (const auto state = m_state.lock())
                state->unsubscribe(m_type_id, m_token);
        }

        m_state.reset();
        m_type_id = event_base::invalid_type_id();
        m_token = 0u;
        m_active.reset();
    }

    bool subscription::valid() const noexcept
    {
        return m_token != 0u && !m_state.expired();
    }

} // namespace catalyst::core
