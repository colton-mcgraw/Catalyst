#include "../detail_backend.hpp"

// Headless backend: no gamepad slots, nothing to read.
namespace catalyst::input::detail
{

    const char *backend_name()
    {
        return "null";
    }

    std::size_t gamepad_capacity() noexcept
    {
        return 0;
    }

    bool read_gamepad(gamepad_id, gamepad_state &) noexcept
    {
        return false;
    }

    bool set_gamepad_rumble(gamepad_id, double, double) noexcept
    {
        return false;
    }

} // namespace catalyst::input::detail
