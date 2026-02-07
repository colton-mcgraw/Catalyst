/**
 * @file event.hpp
 * @brief Defines the base event class and related concepts for the Catalyst framework's event system. The event_base class serves as the base for all events in the system, providing a common interface for event type identification. The event template class allows for the creation of specific event types with unique type IDs. Additionally, several concepts are defined to ensure that event types and callbacks meet the necessary requirements for use within the event system. These components work together to provide a flexible and efficient event-driven architecture for building responsive applications using the Catalyst framework.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <concepts>
#include <type_traits>

/**
 * @namespace catalyst::core
 * @brief The catalyst::core namespace contains core functionalities of the Catalyst framework, including event handling and dispatching.
 */
namespace catalyst::core
{

    /**
     * @typedef event_type_id
     * @brief A type alias for a unique identifier used to distinguish different event types in the Catalyst event system. This identifier is typically implemented as a size_t and is assigned to each event type when it is registered. The event_type_id allows the event system to efficiently identify and manage different types of events when they are published and dispatched to handlers.
     */
    using event_type_id = std::size_t;

    /**
     * @class event_base
     * @brief The base class for all events in the Catalyst event system. This class provides a common interface for event type identification and serves as the base for all specific event types. Each event type that derives from event_base will have a unique type ID, allowing the event system to efficiently manage and dispatch events based on their types.
     */
    class event_base
    {
    public:
        /**
         * @typedef timestamp_t
         * @brief A type alias for a timestamp representing the time at which an event was created or occurred. This timestamp is typically implemented using std::chrono::steady_clock::time_point, which provides a high-resolution clock that is suitable for measuring time intervals in the context of events. The timestamp can be used to track when events occur and to manage event timing and ordering within the event system.
         */
        using timestamp_t = std::chrono::steady_clock::time_point;

    public:
        /**
         * @fn event_base
         * @brief Default constructor for the event_base class. This constructor initializes the event with a default type ID, which is typically set to an invalid value. Derived event types will override this with their own unique type IDs.
         * @details The default constructor initializes the event with an invalid type ID, which can be used to indicate that the event has not been properly initialized or registered. Derived event types will provide their own unique type IDs by calling the protected constructor with a valid type ID. This allows the event system to distinguish between different event types when they are published and dispatched.
         */
        virtual ~event_base() = default;

        /**
         * @fn timestamp
         * @brief Returns the timestamp representing the time at which this event instance was created or occurred. This can be used to track when events occur and to manage event timing and ordering within the event system. The timestamp is typically initialized to the current time when the event is constructed.
         * @return The timestamp representing the time at which this event instance was created or occurred. This can be used to track when events occur and to manage event timing and ordering within the event system.
         */
        inline const timestamp_t& get_timestamp() const noexcept { return timestamp; }

        /**
         * @fn type_id
         * @brief Returns the unique type ID associated with this event instance. This function allows the event system to identify the specific type of the event when it is published and dispatched to handlers. The type ID is typically assigned when the event is constructed and is used by the event system to manage and dispatch events based on their types.
         * @return The unique type ID associated with this event instance. This is used by the event system to identify the specific type of the event when it is published and dispatched to handlers.
         */
        inline const event_type_id& type_id() const noexcept { return m_type_id; }

        /**
         * @fn invalid_type_id
         * @brief Returns a special value representing an invalid event type ID. This value can be used to indicate that an event has not been properly initialized or registered with the event system. It serves as a sentinel value to help identify cases where an event type ID is not valid or has not been assigned.
         * @return A special value representing an invalid event type ID. This can be used to indicate that an event has not been properly initialized or registered with the event system.
         * @details This function returns a special value that can be used as a sentinel to identify cases where an event type ID is not valid or has not been assigned. Derived event types will typically have their own unique type IDs, and this invalid type ID can be used to check for uninitialized or improperly registered events in the system. Compiler optimizations should ensure that comparisons against this invalid type ID are efficient, as it is a compile-time constant.
         */
        inline static constexpr event_type_id invalid_type_id() noexcept { return 0u; }

    protected:
        /**
         * @fn event_base
         * @brief Protected constructor for the event_base class. This constructor is used by derived event types to initialize the base class with a specific type ID. The type ID is typically assigned when the event is constructed and is used by the event system to manage and dispatch events based on their types.
         * @param id The unique type ID to be associated with this event instance. This is used by the event system to identify the specific type of the event when it is published and dispatched to handlers. Derived event types will call this constructor with their own unique type IDs to ensure that each event type has a distinct identifier in the system.
         * @details Derived event types will call this constructor with their own unique type IDs to ensure that each event type has a distinct identifier in the system. This allows the event system to efficiently manage and dispatch events based on their types when they are published.
         */
        inline explicit event_base(event_type_id id) noexcept : m_type_id(id) {}

        /**
         * @fn register_type_id
         * @brief Registers a new unique type ID for an event type. This function is typically called by the event template class to assign a unique type ID to each specific event type. The function uses an atomic counter to ensure that each registered type ID is unique and thread-safe. The returned type ID can then be used by the event system to identify and manage events of that type when they are published and dispatched.
         * @return A new unique type ID for an event type. This is typically called by the event template class to assign a unique type ID to each specific event type.
         * @details The function uses an atomic counter to ensure that each registered type ID is unique and thread-safe. This allows the event system to efficiently manage and dispatch events based on their types when they are published, even in multi-threaded environments.
         */
        inline static event_type_id register_type_id() noexcept
        {
            return s_next_type_id.fetch_add(1u, std::memory_order_relaxed);
        }

    private:
        /**
         * @var m_type_id
         * @brief The unique type ID associated with this event instance. This is used by the event system to identify the specific type of the event when it is published and dispatched to handlers. The type ID is typically assigned when the event is constructed and is used by the event system to manage and dispatch events based on their types.
         */
        event_type_id m_type_id;

        /**
         * @var timestamp
         * @brief A timestamp representing the time at which this event instance was created or occurred. This can be used to track when events occur and to manage event timing and ordering within the event system. The timestamp is typically initialized to the current time when the event is constructed.
         */
        const timestamp_t timestamp = std::chrono::steady_clock::now();

        /**
         * @var s_next_type_id
         * @brief A static atomic counter used to generate unique type IDs for event types. This counter is incremented each time a new type ID is registered, ensuring that each event type receives a unique identifier. The use of an atomic counter allows for thread-safe registration of event types in multi-threaded environments
         */
        inline static std::atomic<event_type_id> s_next_type_id{1u};
    };

    /**
     * @class event
     * @brief A template class for defining specific event types in the Catalyst event system. This class derives from event_base and provides a unique type ID for each specific event type. By using this template, developers can easily create new event types by simply instantiating the template with their desired event data. Each specialization of the event template will have its own unique type ID, allowing the event system to efficiently manage and dispatch events based on their types when they are published.
     * @tparam T The type of data associated with this event. This can be any type that represents the information relevant to the event being defined. The event template allows for flexibility in defining events with different types of data while still providing a common interface for event handling through the base class. By using the event template, developers can easily create new event types by simply instantiating the template with their desired event data, and each specialization will have its own unique type ID for efficient management and dispatching within the event system.
     * @details Each specialization of the event template will have its own unique type ID, allowing the event system to efficiently manage and dispatch events based on their types when they are published. This design allows for flexibility in defining events with different types of data while still providing a common interface for event handling through the base class.
     */
    template <typename T>
    class event : public event_base
    {
    public:
        /**
         * @fn event
         * @brief Default constructor for the event class. This constructor initializes the event with a unique type ID that is specific to the event type T. The type ID is generated by calling the register_type_id function from the base class, ensuring that each specialization of the event template has its own unique identifier in the event system.
         * @details The constructor initializes the event with a unique type ID that is specific to the event type T. This allows the event system to efficiently manage and dispatch events based on their types when they are published, as each specialization of the event template will have its own unique identifier in the system.
         */
        inline event() noexcept : event_base(s_type_id) {}
        /**
         * @fn type_id
         * @brief Returns the unique type ID associated with this event type T. This function allows the event system to identify the specific type of the event when it is published and dispatched to handlers. The type ID is generated by calling the register_type_id function from the base class, ensuring that each specialization of the event template has its own unique identifier in the event system.
         * @return The unique type ID associated with this event type T. This is used by the event system to identify the specific type of the event when it is published and dispatched to handlers.
         * @details The type ID is generated by calling the register_type_id function from the base class, ensuring that each specialization of the event template has its own unique identifier in the event system. This allows the event system to efficiently manage and dispatch events based on their types when they are published.
         */
        inline static event_type_id type_id() noexcept { return s_type_id; }

    private:
        /**
         * @var s_type_id
         * @brief A static constant that holds the unique type ID for this specific event type T. This type ID is generated by calling the register_type_id function from the base class, ensuring that each specialization of the event template has its own unique identifier in the event system. The s_type_id is used by the event system to efficiently manage and dispatch events based on their types when they are published.
         */
        inline static const event_type_id s_type_id = event_base::register_type_id();
    };

    /**
     * @concept event_type
     * @brief A concept that checks if a type is a valid event type in the Catalyst event system. A type satisfies this concept if it is derived from the event_base class, which is the base class for all events in the system. This concept is used to ensure that only valid event types are used in the event system, allowing for type safety and proper management of events when they are published and dispatched to handlers.
     */
    template <typename T>
    concept event_type = std::derived_from<std::remove_cvref_t<T>, event_base>;

    /**
     * @concept registered_event
     * @brief A concept that checks if a type is a registered event type in the Catalyst event system. A type satisfies this concept if it meets the requirements of being an event type (derived from event_base) and also provides a static member function type_id() that returns a valid event_type_id. This concept is used to ensure that only registered event types, which have unique type IDs, are used in the event system, allowing for proper management and dispatching of events based on their types.
     */
    template <typename T>
    concept registered_event = event_type<T> && requires {
        { std::remove_cvref_t<T>::type_id() } -> std::same_as<event_type_id>;
    };

    /**
     * @concept event_callback_for
     * @brief A concept that checks if a given callback type is compatible with a specific event type in the Catalyst event system. A type satisfies this concept if it is an event type and if the callback can be invoked with a const reference to the event type. This concept is used to ensure that only compatible callbacks are subscribed to events, allowing for type safety and proper invocation of handlers when events are published and dispatched to handlers.
     */
    template <typename Callback, typename T>
    concept event_callback_for = event_type<T> && std::invocable<Callback &, const std::remove_cvref_t<T> &>;

    /**
     * @concept function_pointer
     * @brief A concept that checks if a given type is a function pointer. A type satisfies this concept if it is a pointer to a function type, which can be used as a callback in the event system. This concept is used to differentiate between function pointers and other callable types, allowing for proper handling of subscriptions in the event system.
     */
    template <typename T>
    concept function_pointer = std::is_pointer_v<std::remove_cvref_t<T>> &&
                               std::is_function_v<std::remove_pointer_t<std::remove_cvref_t<T>>>;

} // namespace catalyst::core