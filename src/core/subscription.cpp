#include <catalyst/core/subscription.hpp>

#include <catalyst/core/dispatcher.hpp>

namespace catalyst::core
{

    void subscription::reset() noexcept
    {
        if (m_active)
            m_active->store(false, std::memory_order_relaxed);

        if (m_dispatcher != nullptr && m_type_id != event_base::invalid_type_id() && m_token != 0u)
            m_dispatcher->unsubscribe(m_type_id, m_token);

        m_dispatcher = nullptr;
        m_type_id = event_base::invalid_type_id();
        m_token = 0u;
        m_active.reset();
    }

} // namespace catalyst::core
