#include <catalyst/core/subscription.hpp>

#include <catalyst/core/dispatcher.hpp>

namespace catalyst::core
{

    subscription::subscription(
        dispatcher *dispatcher_ptr,
        event_type_id type_id,
        std::size_t token,
        std::shared_ptr<std::atomic_bool> active) noexcept
        : m_dispatcher(dispatcher_ptr),
          m_type_id(type_id),
          m_token(token),
          m_active(std::move(active))
    {
    }

    subscription::subscription(subscription &&other) noexcept
        : m_dispatcher(other.m_dispatcher),
          m_type_id(other.m_type_id),
          m_token(other.m_token),
          m_active(std::move(other.m_active))
    {
        other.m_dispatcher = nullptr;
        other.m_type_id = event_base::invalid_type_id();
        other.m_token = 0u;
    }

    subscription &subscription::operator=(subscription &&other) noexcept
    {
        if (this == &other)
            return *this;

        reset();

        m_dispatcher = other.m_dispatcher;
        m_type_id = other.m_type_id;
        m_token = other.m_token;
        m_active = std::move(other.m_active);

        other.m_dispatcher = nullptr;
        other.m_type_id = event_base::invalid_type_id();
        other.m_token = 0u;

        return *this;
    }

    subscription::~subscription()
    {
        reset();
    }

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

    bool subscription::valid() const noexcept
    {
        return m_dispatcher != nullptr && m_type_id != event_base::invalid_type_id() && m_token != 0u;
    }

} // namespace catalyst::core
