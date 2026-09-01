#include "test_common.hpp"
#include "test_static_init_event.hpp"

#include <catalyst/core/event.hpp>

// Defined in test_static_init_tu.cpp: captured during that translation unit's dynamic initialisation.
catalyst::core::event_type_id id_captured_in_other_tu();

// Captured during this translation unit's dynamic initialisation.
static const catalyst::core::event_type_id g_captured_here = catalyst::tests::static_init_event::type_id();

int main()
{
    using catalyst::core::event_base;
    using catalyst::tests::static_init_event;

    const auto runtime_id = static_init_event::type_id();

    CT_REQUIRE(runtime_id != event_base::invalid_type_id());
    CT_REQUIRE(g_captured_here == runtime_id);
    CT_REQUIRE(id_captured_in_other_tu() == runtime_id);

    static_init_event e;
    CT_REQUIRE(e.type_id() == runtime_id);
    return 0;
}
