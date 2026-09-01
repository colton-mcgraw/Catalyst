/**
 * @file subscription.hpp
 * @brief RAII handle returned by dispatcher::subscribe. Destroying or resetting it removes the handler.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include "event.hpp"

#include <atomic>
#include <cstddef>
#include <memory>

namespace catalyst::core
{
    class dispatcher;

    namespace detail
    {
        class dispatcher_state;
    }

    /**
     * @class subscription
     * @brief Owns one handler registration. Move-only; reset() or destruction unsubscribes.
     *
     * The handle refers to the dispatcher's shared state through a weak pointer, so it is safe to destroy a subscription
     * after the dispatcher it came from has been destroyed (it simply becomes a no-op). It is also safe to reset a
     * subscription from inside a handler, including the handler it owns.
     *
     * Ownership must be released on the dispatcher's owner thread (see dispatcher). Flipping the active flag is atomic, but
     * removing the entry touches the dispatcher's handler tables.
     */
    class subscription
    {
    public:
        subscription() = default;

        subscription(const subscription &) = delete;
        subscription &operator=(const subscription &) = delete;

        subscription(subscription &&other) noexcept;
        subscription &operator=(subscription &&other) noexcept;

        ~subscription();

        /** @brief Unsubscribes the handler. Safe to call repeatedly and on an empty handle. */
        void reset() noexcept;

        /** @brief True while this handle owns a registration on a still-living dispatcher. */
        [[nodiscard]] bool valid() const noexcept;

        /** @brief The event type this subscription listens for, or invalid_type_id() if empty. */
        [[nodiscard]] event_type_id type_id() const noexcept { return m_type_id; }

        /** @brief The dispatcher-unique token of this registration, or 0 if empty. */
        [[nodiscard]] std::size_t token() const noexcept { return m_token; }

    private:
        friend class dispatcher;

        subscription(std::weak_ptr<detail::dispatcher_state> state,
                     event_type_id type_id,
                     std::size_t token,
                     std::shared_ptr<std::atomic_bool> active) noexcept;

        std::weak_ptr<detail::dispatcher_state> m_state;
        event_type_id m_type_id = event_base::invalid_type_id();
        std::size_t m_token = 0u;
        std::shared_ptr<std::atomic_bool> m_active;
    };

} // namespace catalyst::core
