#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <typeindex>
#include <variant>

namespace catalyst::events
{

    using event_type_t = std::size_t;

    /**
     * @struct tagged
     * @brief A template struct used to tag events with a unique event type identifier.
     * @tparam Id The unique identifier for the event type.
     */
    template <event_type_t Id>
    struct tagged
    {
        static constexpr event_type_t tag = Id;
    };

    /**
     * @struct static_event_tag
     * @brief A template struct used to retrieve the static event tag of an event type, if it exists.
     * @tparam Event The event type to query for a static event tag.
     */
    template <typename Event>
    struct static_event_tag
    {
    };

    /**
     * @concept has_tag
     * @brief A concept that checks if an event type has a static tag.
     * @tparam Event The event type to check.
     */
    template <typename Event>
    concept has_tag = requires {
        { Event::tag } -> std::convertible_to<event_type_t>;
    };

    /**
     * @struct has_static_tag
     * @brief A template struct used to determine if an event type has a static event tag.
     * @tparam Event The event type to query.
     */
    template <typename Event>
        requires has_tag<Event>
    struct static_event_tag<Event>
    {
        static constexpr event_type_t value = Event::tag;
    };

    /**
     * @struct has_static_tag
     * @brief A template struct used to determine if an event type has a static event tag.
     * @tparam Event The event type to query.
     */
    template <typename Event>
        struct has_static_tag : std::bool_constant < requires
    {
        {
            static_event_tag<Event>::value
        } -> std::convertible_to<event_type_t>;
    } > {};

    /**
     * @variable has_static_tag_v
     * @brief A variable template that provides a convenient way to check if an event type has a static event tag.
     * @tparam Event The event type to query.
     */
    template <typename Event>
    inline constexpr bool has_static_tag_v = has_static_tag<Event>::value;

    /**
     * @typedef event_key
     * @brief A variant type that can hold either an event type identifier or a type index.
     */
    using event_key = std::variant<event_type_t, std::type_index>;

    /**
     * @variable invalid_event_key
     * @brief A constant representing an invalid event key.
     */
    constexpr event_key invalid_event_key = event_key{std::in_place_index<0>, static_cast<event_type_t>(-1)};

    /**
     * @brief Retrieves the event key for a given event type.
     * @tparam Event The event type to query.
     * @return The event key corresponding to the event type.
     */
    template <typename Event>
    constexpr event_key event_id()
    {
        if constexpr (has_static_tag_v<Event>) // If the event has a static tag, use it as the event key.
            return event_key{std::in_place_index<0>, static_event_tag<Event>::value};
        else // If the event does not have a static tag, use its type index as the event key.
            return event_key{std::in_place_index<1>, typeid(Event)};
    }

    namespace detail
    {
        /**
         * @struct hashed_key
         * @brief An event key together with its hash.
         * @note The bus keys its tables on this rather than on event_key so that the hash is computed
         *       once per event type rather than once per dispatch. That matters more than it looks:
         *       on the MSVC STL, type_index::hash_code() hashes the mangled type name character by
         *       character every time it is called.
         */
        struct hashed_key
        {
            event_key key;
            std::size_t hash;

            friend constexpr bool operator==(const hashed_key &a, const hashed_key &b) noexcept
            {
                return a.hash == b.hash && a.key == b.key;
            }
        };

        // splitmix64 finaliser: spreads the low bits of small integers (static tags are usually
        // 0, 1, 2, ...) across the whole word so that masking off the low bits picks distinct
        // buckets.
        constexpr std::size_t mix(std::uint64_t x) noexcept
        {
            x ^= x >> 30;
            x *= 0xbf58476d1ce4e5b9ull;
            x ^= x >> 27;
            x *= 0x94d049bb133111ebull;
            x ^= x >> 31;
            return static_cast<std::size_t>(x);
        }

        inline std::size_t hash_key(const event_key &key) noexcept
        {
            if (key.index() == 0)
                return mix(std::get<0>(key));
            return mix(std::get<1>(key).hash_code() ^ 0x9e3779b97f4a7c15ull);
        }

        /**
         * @brief The hashed key of an event type, computed once and then shared by every dispatch,
         * registration and removal of that type.
         * @tparam Event The event type.
         */
        template <typename Event>
        const hashed_key &key_of() noexcept
        {
            if constexpr (has_static_tag_v<Event>)
            {
                // Constant-initialised, so there is no initialisation guard on the hot path.
                static constexpr hashed_key key{event_id<Event>(), mix(static_event_tag<Event>::value)};
                return key;
            }
            else
            {
                static const hashed_key key{event_id<Event>(), hash_key(event_id<Event>())};
                return key;
            }
        }
    } // namespace detail

} // namespace catalyst::events
