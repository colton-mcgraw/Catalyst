#pragma once

#include "event.hpp"
#include "subscription.hpp"

namespace catalyst::core
{

    class dispatcher
    {
    public:
        dispatcher() = default;
        dispatcher(const dispatcher &) = delete;
        dispatcher &operator=(const dispatcher &) = delete;

        template <typename Event_T>
        [[nodiscard]] subscription subscribe(void (*callback)(const Event_T &))
        {
            static_assert(std::is_base_of_v<event_base, Event_T>, "Event_T must derive from catalyst::core::event_base");

            const event_type_id id = Event_T::type_id();
            const std::size_t token = m_next_token++;
            auto active = std::make_shared<std::atomic_bool>(true);

            handler_entry entry;
            entry.token = token;
            entry.active = active;
            entry.invoke = [callback](const event_base &base)
            {
                callback(static_cast<const Event_T &>(base));
            };

            m_handlers[id].push_back(std::move(entry));
            return subscription(this, id, token, std::move(active));
        }

        void publish(const event_base &e)
        {
            const auto found = m_handlers.find(e.type_id());
            if (found == m_handlers.end())
                return;

            ++m_dispatch_depth;
            for (const auto &handler : found->second)
            {
                if (handler.active && handler.active->load(std::memory_order_relaxed))
                    handler.invoke(e);
            }
            --m_dispatch_depth;

            if (m_dispatch_depth == 0u)
                cleanup_dead();
        }

        template <typename Event_T>
        void publish(const Event_T &e)
        {
            static_assert(std::is_base_of_v<event_base, Event_T>, "Event_T must derive from catalyst::core::event_base");
            publish(static_cast<const event_base &>(e));
        }

        void unsubscribe(event_type_id id, std::size_t token) noexcept
        {
            const auto found = m_handlers.find(id);
            if (found == m_handlers.end())
                return;

            for (auto &handler : found->second)
            {
                if (handler.token == token)
                {
                    if (handler.active)
                        handler.active->store(false, std::memory_order_relaxed);
                    break;
                }
            }

            if (m_dispatch_depth == 0u)
                cleanup_dead();
        }

    private:
        struct handler_entry
        {
            std::size_t token = 0u;
            std::shared_ptr<std::atomic_bool> active;
            std::function<void(const event_base &)> invoke;
        };

        void cleanup_dead()
        {
            for (auto it = m_handlers.begin(); it != m_handlers.end();)
            {
                auto &vec = it->second;
                vec.erase(std::remove_if(vec.begin(), vec.end(), [](const handler_entry &h)
                                         { return !h.active || !h.active->load(std::memory_order_relaxed); }),
                          vec.end());

                if (vec.empty())
                    it = m_handlers.erase(it);
                else
                    ++it;
            }
        }

        std::unordered_map<event_type_id, std::vector<handler_entry>> m_handlers;
        std::size_t m_next_token = 1u;
        std::size_t m_dispatch_depth = 0u;
    };

}