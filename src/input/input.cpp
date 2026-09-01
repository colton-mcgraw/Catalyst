#include <catalyst/input/input.hpp>

#include "detail_backend.hpp"

namespace catalyst::input
{
    namespace
    {
        core::event_sink *g_event_sink = nullptr;
    }

    const char *module_name()
    {
        return detail::backend_name();
    }

    void set_event_sink(core::event_sink *sink) noexcept
    {
        g_event_sink = sink;
    }

    core::event_sink *get_event_sink() noexcept
    {
        return g_event_sink;
    }

} // namespace catalyst::input
