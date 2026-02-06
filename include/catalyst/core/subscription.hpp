#pragma once

#include "event.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace catalyst::core
{

    class subscription
    {
    public:
        subscription() = default;

        subscription(const subscription &) = delete;
        subscription &operator=(const subscription &) = delete;

        subscription(subscription &&other) noexcept
            : m_dispatcher(other.m_dispatcher),
              m_type_id(other.m_type_id),
              m_token(other.m_token),
              m_active(std::move(other.m_active))
        {
            other.m_dispatcher = nullptr;
            other.m_type_id = event_base::invalid_type_id();
            other.m_token = 0u;
        }

        subscription &operator=(subscription &&other) noexcept
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

        ~subscription() { reset(); }

        void reset() noexcept;

        [[nodiscard]] bool valid() const noexcept
        {
            return m_dispatcher != nullptr && m_type_id != event_base::invalid_type_id() && m_token != 0u;
        }

    private:
        friend class dispatcher;

        subscription(dispatcher *dispatcher_ptr, event_type_id type_id, std::size_t token,
                     std::shared_ptr<std::atomic_bool> active) noexcept
            : m_dispatcher(dispatcher_ptr), m_type_id(type_id), m_token(token), m_active(std::move(active))
        {
        }

        dispatcher *m_dispatcher = nullptr;
        event_type_id m_type_id = event_base::invalid_type_id();
        std::size_t m_token = 0u;
        std::shared_ptr<std::atomic_bool> m_active;
    };

} // namespace catalyst::core