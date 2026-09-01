#include "test_static_init_event.hpp"

// Plugin-style static registration: the id is read while this TU is being dynamically initialised, before main().
static const catalyst::core::event_type_id g_captured = catalyst::tests::static_init_event::type_id();

catalyst::core::event_type_id id_captured_in_other_tu()
{
    return g_captured;
}
