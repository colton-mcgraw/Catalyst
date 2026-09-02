/**
 * @file event.hpp
 * @brief Defines the base event types used by the Catalyst event system. Every event derives from event_base (usually via the
 * CRTP helper event<T>), which gives it a process-unique type id and an optional timestamp.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

/**
 * @namespace catalyst::core
 * @brief The catalyst::core namespace contains core functionalities of the Catalyst framework, including event handling and dispatching.
 */
namespace catalyst::core
{
    /**
     * @typedef event_type_id
     * @brief Integer identifier assigned to every event type at runtime. Ids are unique within a process but are not stable
     * across runs (they depend on first-use order), so never serialise them.
     * @note Ids are allocated from an inline static counter, so on Windows every DLL that links the core module gets its own
     * counter. Events must not cross a DLL boundary unless the core module itself is built as a shared library.
     */
    using event_type_id = std::size_t;

    /**
     * @typedef coalesce_key
     * @brief Identifies the "slot" an event occupies for the purpose of collapsing redundant events in a queue.
     * @details Some events carry only a current value rather than an occurrence: the position of a window, the size of a
     * window, the state of an axis. When several of them are queued for the same slot before anything drains the queue, only
     * the newest one carries information and the older ones are dead on arrival. Giving such an event a non-zero coalesce
     * key tells event_queue that it may replace the event already at the back of the queue when that event has the same
     * type id and the same key.
     *
     * The key is entirely the producer's to choose; it only has to distinguish the streams that must not be merged with one
     * another. A window backend uses the window id, so a resize of one window never swallows the resize of another.
     * @note Never give a key to an event that describes an occurrence rather than a value. Key presses, button presses,
     * characters and close requests are each meaningful individually and must never be collapsed.
     */
    using coalesce_key = std::uint64_t;

    /** @brief The key value that means "this event must never be coalesced". It is the default for every event. */
    inline constexpr coalesce_key no_coalescing = 0u;

    /**
     * @class event_base
     * @brief Polymorphic base for all events. Carries the runtime type id and an optional timestamp.
     *
     * The timestamp is *not* taken automatically: reading the clock costs as much as a whole dispatch, and many events are
     * published immediately and never inspected. Call stamp() (event_queue and the platform backends do this for you when an
     * event is queued) or set_timestamp() when you need one; has_timestamp() tells you whether it was set.
     */
    class event_base
    {
    public:
        using timestamp_t = std::chrono::steady_clock::time_point;

        virtual ~event_base() = default;

        /** @brief Returns the timestamp, or a default-constructed time_point if the event was never stamped. */
        [[nodiscard]] const timestamp_t &get_timestamp() const noexcept { return m_timestamp; }

        /** @brief True if stamp() or set_timestamp() has been called on this event. */
        [[nodiscard]] bool has_timestamp() const noexcept { return m_timestamp != timestamp_t{}; }

        /** @brief Records the current steady_clock time on the event. */
        void stamp() noexcept { m_timestamp = std::chrono::steady_clock::now(); }

        /** @brief Sets an explicit timestamp (e.g. one captured by a producer on another thread). */
        void set_timestamp(timestamp_t t) noexcept { m_timestamp = t; }

        /** @brief The coalescing slot of this event, or no_coalescing (the default) if it must never be collapsed. */
        [[nodiscard]] coalesce_key get_coalesce_key() const noexcept { return m_coalesce_key; }

        /**
         * @brief Marks this event as coalescible within @p key. See coalesce_key for when this is and is not appropriate.
         * @note Only event_queue acts on this. An event published straight to a dispatcher is delivered synchronously and
         * has nothing to be collapsed against, so setting a key on it is harmless and does nothing.
         */
        void set_coalesce_key(coalesce_key key) noexcept { m_coalesce_key = key; }

        /** @brief Runtime type id of the concrete event. */
        [[nodiscard]] event_type_id type_id() const noexcept { return m_type_id; }

        /** @brief The id value that is never assigned to a real event type. */
        [[nodiscard]] static constexpr event_type_id invalid_type_id() noexcept { return 0u; }

    protected:
        explicit event_base(event_type_id id) noexcept : m_type_id(id) {}

        /** @brief Allocates a fresh, process-unique event type id. */
        static event_type_id register_type_id() noexcept
        {
            return s_next_type_id.fetch_add(1u, std::memory_order_relaxed);
        }

    private:
        event_type_id m_type_id;
        timestamp_t m_timestamp{};
        coalesce_key m_coalesce_key = no_coalescing;

        inline static std::atomic<event_type_id> s_next_type_id{1u};
    };

    /**
     * @class event
     * @brief CRTP helper that gives a derived type a static type_id(). Usage: `struct my_event : event<my_event> { ... };`
     *
     * The id lives in a function-local static, so it is initialised on first use and is safe to read during static
     * initialisation of other translation units (a static data member of a class template would be *unordered* and could be
     * observed as invalid_type_id()).
     */
    template <typename T>
    class event : public event_base
    {
    public:
        event() noexcept : event_base(type_id()) {}

        [[nodiscard]] static event_type_id type_id() noexcept
        {
            static const event_type_id id = event_base::register_type_id();
            return id;
        }
    };

    /** @brief Satisfied by any type derived from event_base. */
    template <typename T>
    concept event_type = std::derived_from<std::remove_cvref_t<T>, event_base>;

    /** @brief Satisfied by event types that expose a static type_id() (anything derived from event<T>). */
    template <typename T>
    concept registered_event = event_type<T> && requires {
        { std::remove_cvref_t<T>::type_id() } -> std::same_as<event_type_id>;
    };

    /**
     * @brief Satisfied by callables that accept `const T&`. The callable may return void (never consumes the event) or a
     * value convertible to bool, where `true` means the event was consumed and propagation to lower-priority handlers stops.
     */
    template <typename Callback, typename T>
    concept event_callback_for =
        event_type<T> &&
        std::invocable<Callback &, const std::remove_cvref_t<T> &> &&
        (std::is_void_v<std::invoke_result_t<Callback &, const std::remove_cvref_t<T> &>> ||
         std::convertible_to<std::invoke_result_t<Callback &, const std::remove_cvref_t<T> &>, bool>);

    /** @brief Satisfied by plain function pointers. Used to keep the function-pointer subscribe overloads unambiguous. */
    template <typename T>
    concept function_pointer = std::is_pointer_v<std::remove_cvref_t<T>> &&
                               std::is_function_v<std::remove_pointer_t<std::remove_cvref_t<T>>>;

} // namespace catalyst::core
