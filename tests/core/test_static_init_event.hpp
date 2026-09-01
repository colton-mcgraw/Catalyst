#pragma once

#include <catalyst/core/event.hpp>

namespace catalyst::tests
{
    struct static_init_event final : core::event<static_init_event>
    {
    };
} // namespace catalyst::tests
