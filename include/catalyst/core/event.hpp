#pragma once

#include <atomic>
#include <cstddef>

namespace catalyst::core
{

    using event_type_id = std::size_t;

    class event_base
    {
    public:
        virtual ~event_base() = default;

        inline event_type_id type_id() const noexcept { return m_type_id; }

        inline static constexpr event_type_id invalid_type_id() noexcept { return 0u; }

    protected:
        inline explicit event_base(event_type_id id) noexcept : m_type_id(id) {}

        // Registers and returns a new unique event_type_id. Each call returns a different ID.
        // This is used by derived event<> classes to get their unique type IDs. Thread-safe.
        inline static event_type_id register_type_id() noexcept
        {
            return s_next_type_id.fetch_add(1u, std::memory_order_relaxed);
        }

    private:
        event_type_id m_type_id;

        inline static std::atomic<event_type_id> s_next_type_id{ 1u };
    };

    template <typename Event_T>
    class event : public event_base
    {
    public:
        inline event() noexcept : event_base(s_type_id) {}

        // Returns the unique type ID for this template specialization of event<>, not a UID.
        inline static event_type_id type_id() noexcept { return s_type_id; }

    private:
        // Creates a unique type ID for this template specialization of event<>
        inline static const event_type_id s_type_id = event_base::register_type_id();
    };

} // namespace catalyst::core