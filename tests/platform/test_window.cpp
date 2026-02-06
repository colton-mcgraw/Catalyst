#include <catalyst/platform/platform.hpp>
#include <catalyst/platform/window.hpp>

#include <cassert>
#include <string>

int main()
{
    using namespace catalyst;

    platform::window_desc a{};
    a.title = "Test A";
    a.width_px = 320;
    a.height_px = 240;
    a.visible = false;

    platform::window_desc b{};
    b.title = "Test B";
    b.width_px = 640;
    b.height_px = 480;
    b.visible = false;

    auto w1 = platform::create_window(a);
    auto w2 = platform::create_window(b);

    assert(w1);
    assert(w2);
    assert(w1.id() != w2.id());

    // Make sure the backend is responsive.
    platform::pump_events();

    // Validate queries.
    {
        const auto r1 = platform::client_rect_px(w1);
        const auto r2 = platform::client_rect_px(w2);

        assert(r1.size().x > 0);
        assert(r1.size().y > 0);
        assert(r2.size().x > 0);
        assert(r2.size().y > 0);

        const float s1 = platform::dpi_scale(w1);
        const float s2 = platform::dpi_scale(w2);
        assert(s1 > 0.0f);
        assert(s2 > 0.0f);

        const auto nh = platform::get_native_handle(w1);
        const std::string backend = platform::module_name();
        if (backend == "win32")
        {
            assert(nh.kind == platform::native_handle_kind::win32_hwnd);
            assert(nh.handle != nullptr);
        }
        else
        {
            assert(nh.kind == platform::native_handle_kind::none);
        }
    }

    // Drain initial events (resized/dpi).
    {
        platform::event e;
        for (int i = 0; i < 16; ++i)
        {
            if (!platform::poll_event(e))
                break;
        }
    }

    platform::destroy_window(w2);
    platform::destroy_window(w1);

    return 0;
}
