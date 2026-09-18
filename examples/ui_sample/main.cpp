/*
 * @file main.cpp
 * @brief A small retained UI drawn in a real window: a styled tree laid out against the swapchain, painted into a draw
 * list, submitted through the renderer bridge, and driven by the mouse and keyboard.
 * @details Builds a header, a row of three buttons, a card of wrapped text, a status line and a translucent toast from
 * the public `catalyst::ui` API, then runs a frame loop that re-lays the tree out whenever the window resizes or a
 * style or label changes, paints it into a `render_batch`, and hands that batch to `ui::renderer`: `prepare` before
 * the swapchain pass draws the toast's group-opacity layer offscreen, `render` inside it uploads the batch and records
 * one scissored draw per command plus one composite quad for the layer. Text is real: `font_provider.hpp` is a
 * `ui::text_provider` over stb_truetype that rasterises glyphs on demand into an `r8` coverage atlas, and whenever
 * that atlas changes the loop uploads it as a sampled texture and registers it with the renderer under the key the
 * glyphs carry. The font comes from the command line (`catalyst_ui_sample path/to/font.ttf`) or from a short list of
 * system faces; with none found the module's `null_text_provider` draws boxes instead. The input side is the dozen
 * lines that turn `catalyst::input` mouse and key events into `ui::interaction` calls, and the `click_event`,
 * `pointer_enter_event` and `focus_gained_event` the tree publishes back on the same bus. Hover recolours a button,
 * clicking "Increment" or "Reset" rewrites the counter label, "G" flips the toast between group and per-node opacity,
 * "Quit" or Escape ends the loop.
 * License: MIT (see LICENSE).
 */

// The MSVC CRT deprecates getenv in favour of _dupenv_s; the font search below reads WINDIR with it, the same way
// the logging terminal sink reads NO_COLOR and TERM, so the same opt-out applies. Must precede every include.
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "font_provider.hpp"

#include <catalyst/events/bus.hpp>
#include <catalyst/input/input.hpp>
#include <catalyst/logging/logging.hpp>
#include <catalyst/platform/window.hpp>
#include <catalyst/rendering/rendering.hpp>
#include <catalyst/ui/renderer.hpp>
#include <catalyst/ui/ui.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace platform = catalyst::platform;
namespace input = catalyst::input;
namespace events = catalyst::events;
namespace logging = catalyst::logging;
namespace rendering = catalyst::rendering;
namespace ui = catalyst::ui;

using namespace std::chrono_literals;

namespace
{
    /** @brief Names this example in the log's category column. */
    struct example_log
    {
        static constexpr const char *name = "ui_sample";
    };

    // ---- palette --------------------------------------------------------------------------------
    // ui::color holds linear values and the swapchain is sRGB, so it encodes on write. These colours were picked as
    // sRGB bytes, the way design tools present them, so decode them once here.

    float srgb_to_linear(std::uint8_t byte) noexcept
    {
        const float c = static_cast<float>(byte) / 255.0f;
        return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }

    ui::color srgb(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept
    {
        return ui::color::rgba(srgb_to_linear(r), srgb_to_linear(g), srgb_to_linear(b), static_cast<float>(a) / 255.0f);
    }

    namespace palette
    {
        const ui::color background = srgb(24, 26, 32);
        const ui::color surface = srgb(34, 37, 46);
        const ui::color border = srgb(60, 65, 78);
        const ui::color accent = srgb(88, 140, 255);
        const ui::color accent_hover = srgb(120, 165, 255);
        const ui::color button = srgb(48, 52, 64);
        const ui::color button_hover = srgb(66, 72, 88);
        const ui::color danger = srgb(200, 70, 70);
        const ui::color danger_hover = srgb(230, 95, 95);
        const ui::color text = srgb(230, 232, 238);
        const ui::color text_dim = srgb(150, 156, 170);
    } // namespace palette

    // ---- the screen -----------------------------------------------------------------------------

    /** @brief A button the sample knows by name, with the colours it swaps between on hover. */
    struct button
    {
        ui::node node = ui::null_node;
        const char *name = "";
        ui::color fill{};
        ui::color hover{};
    };

    /**
     * @brief Everything the sample keeps alive for the tree: the tree itself, the text the nodes point at, and the
     * handles the event handlers need to find their way back to a node.
     * @details `texts` is a deque because `text_content::attach` hands the tree a pointer to the content, and a deque
     * keeps its elements in place as more are added. The tree stores only pointers, so the content must outlive it.
     */
    struct screen
    {
        ui::tree tree;
        ui::text_provider *provider = nullptr; ///< The font every text node is set with; chosen before `build`.
        std::deque<ui::text_content> texts;
        ui::node root = ui::null_node;
        ui::text_content *counter = nullptr;
        ui::node counter_node = ui::null_node;
        ui::text_content *status = nullptr;
        ui::node status_node = ui::null_node;
        ui::node toast = ui::null_node;
        ui::text_content *toast_label = nullptr;
        ui::node toast_node = ui::null_node;
        std::vector<button> buttons;
        std::vector<std::pair<ui::node, const char *>> names;

        /** @brief Creates a text leaf under `parent`. */
        ui::node text(ui::node parent, std::string s, ui::color c, bool wrap = false, float size_px = 0.0f)
        {
            const ui::node n = tree.create_child(parent);
            ui::text_content &content = texts.emplace_back();
            content.text = std::move(s);
            content.style.color = c;
            content.style.wrap = wrap;
            content.style.size_px = size_px;
            content.provider = provider;
            content.attach(tree, n);
            if (wrap)
                tree.mutable_style(n).flex_shrink = 1.0f;
            return n;
        }

        /** @brief Creates a flex container under `parent`. */
        ui::node box(ui::node parent, ui::flex_direction direction = ui::flex_direction::row)
        {
            const ui::node n = tree.create_child(parent);
            tree.mutable_style(n).direction = direction;
            return n;
        }

        /** @brief Creates a named button: a rounded box with a label, hittable as one node. */
        ui::node add_button(ui::node parent, const char *label, ui::color fill, ui::color hover)
        {
            const ui::node n = box(parent);
            ui::style &st = tree.mutable_style(n);
            st.height = ui::px(32.0f);
            st.padding = ui::edges_length::symmetric(ui::px(14.0f), ui::px(0.0f));
            st.background = fill;
            st.border = ui::edges_length::all(ui::px(1.0f));
            st.border_color = palette::border;
            st.border_radius = ui::corners_length::all(ui::px(6.0f));
            st.align_items = ui::align::center;
            st.justify_content = ui::justify::center;

            // The label sits inside the button but must not steal the hit: the pointer over the text should still
            // report the button as the target, so the handlers below can look it up by node.
            const ui::node label_node = text(n, label, palette::text);
            tree.mutable_style(label_node).pointer_events = ui::pointer_mode::none;

            buttons.push_back(button{n, label, fill, hover});
            names.emplace_back(n, label);
            return n;
        }

        /** @brief The name registered for a node, for the log. */
        [[nodiscard]] const char *name_of(ui::node n) const noexcept
        {
            for (const auto &[handle, name] : names)
                if (handle == n)
                    return name;
            return ui::is_null(n) ? "-" : "(unnamed)";
        }

        [[nodiscard]] button *button_of(ui::node n) noexcept
        {
            for (button &b : buttons)
                if (b.node == n)
                    return &b;
            return nullptr;
        }

        void build()
        {
            root = tree.create();
            {
                ui::style &st = tree.mutable_style(root);
                st.direction = ui::flex_direction::column;
                // Fill the viewport. An auto-sized root is only as tall as its content, and the card's
                // flex_grow would then have nothing to grow into.
                st.width = ui::vw(100.0f);
                st.height = ui::vh(100.0f);
                st.background = palette::background;
                st.padding = ui::edges_length::all(ui::px(16.0f));
                st.set_gap(ui::px(12.0f));
            }
            names.emplace_back(root, "root");

            // Header: a title on the left, a hint on the right.
            const ui::node header = box(root);
            {
                ui::style &st = tree.mutable_style(header);
                st.height = ui::px(48.0f);
                st.padding = ui::edges_length::symmetric(ui::px(16.0f), ui::px(0.0f));
                st.background = palette::surface;
                st.border_radius = ui::corners_length::all(ui::px(8.0f));
                st.align_items = ui::align::center;
                st.justify_content = ui::justify::space_between;
            }
            names.emplace_back(header, "header");
            text(header, "Catalyst UI sample", palette::text, false, 20.0f);
            text(header, "Escape quits", palette::text_dim);

            // Toolbar: three buttons. The gap is on the container, so the buttons carry no margins.
            const ui::node toolbar = box(root);
            tree.mutable_style(toolbar).set_gap(ui::px(8.0f));
            names.emplace_back(toolbar, "toolbar");
            add_button(toolbar, "Increment", palette::accent, palette::accent_hover);
            add_button(toolbar, "Reset", palette::button, palette::button_hover);
            add_button(toolbar, "Quit", palette::danger, palette::danger_hover);

            // Card: grows to fill whatever height is left, clips its content, and wraps its body text to its width.
            const ui::node card = box(root, ui::flex_direction::column);
            {
                ui::style &st = tree.mutable_style(card);
                st.flex_grow = 1.0f;
                st.min_height = ui::px(0.0f);
                st.padding = ui::edges_length::all(ui::px(12.0f));
                st.background = palette::surface;
                st.border = ui::edges_length::all(ui::px(1.0f));
                st.border_color = palette::border;
                st.border_radius = ui::corners_length::all(ui::px(8.0f));
                st.overflow = ui::overflow_mode::hidden;
                st.set_gap(ui::px(8.0f));
            }
            names.emplace_back(card, "card");
            counter_node = text(card, "Clicks: 0", palette::text, false, 18.0f);
            counter = &texts.back();
            text(card,
                 "This tree is laid out against the swapchain, painted into a render_batch whenever something changes, "
                 "and drawn through ui::renderer every frame. Text is set from a TrueType face by a small "
                 "stb_truetype provider that rasterises glyphs into an r8 atlas, which the renderer samples as a "
                 "coverage mask. Resize the window to see the layout follow it, move over the buttons to see hover, "
                 "and click them to see click events routed through the bus.",
                 palette::text_dim, true);

            // Status line: what the interaction state machine currently thinks.
            const ui::node status_bar = box(root);
            tree.mutable_style(status_bar).height = ui::px(24.0f);
            tree.mutable_style(status_bar).align_items = ui::align::center;
            names.emplace_back(status_bar, "status");
            status_node = text(status_bar, "hover: -   focus: -", palette::text_dim);
            status = &texts.back();

            // Toast: a translucent panel pinned to the bottom-right corner, over the card. Its opacity is group
            // opacity by default, so the label sits solidly on its accent pill and the whole panel fades as one;
            // under opacity_mode::multiply the label and the pill fade separately and show through each other.
            toast = box(root);
            {
                ui::style &st = tree.mutable_style(toast);
                st.position = ui::position_mode::absolute;
                st.inset.right = ui::px(24.0f);
                st.inset.bottom = ui::px(48.0f);
                st.padding = ui::edges_length::all(ui::px(10.0f));
                st.background = palette::surface;
                st.border = ui::edges_length::all(ui::px(1.0f));
                st.border_color = palette::border;
                st.border_radius = ui::corners_length::all(ui::px(8.0f));
                st.opacity = 0.6f;
                st.pointer_events = ui::pointer_mode::none;
            }
            names.emplace_back(toast, "toast");
            const ui::node pill = box(toast);
            {
                ui::style &st = tree.mutable_style(pill);
                st.height = ui::px(32.0f);
                st.padding = ui::edges_length::symmetric(ui::px(12.0f), ui::px(0.0f));
                st.background = palette::accent;
                st.border_radius = ui::corners_length::all(ui::px(16.0f));
                st.align_items = ui::align::center;
            }
            toast_node = text(pill, "group opacity 0.6  (G toggles)", palette::text);
            toast_label = &texts.back();
        }
    };

    // ---- input -> ui bridge ---------------------------------------------------------------------
    // The UI module has its own button and modifier vocabulary so that it does not link the input module. These are
    // the whole translation; a touch or pen source would write its own.

    ui::pointer_button to_ui(input::mouse_button b) noexcept
    {
        switch (b)
        {
        case input::mouse_button::left:
            return ui::pointer_button::left;
        case input::mouse_button::right:
            return ui::pointer_button::right;
        case input::mouse_button::middle:
            return ui::pointer_button::middle;
        case input::mouse_button::x1:
            return ui::pointer_button::extra1;
        case input::mouse_button::x2:
            return ui::pointer_button::extra2;
        default:
            return ui::pointer_button::none;
        }
    }

    ui::modifiers to_ui(input::key_modifiers m) noexcept
    {
        using input::has_modifier;
        using input::key_modifiers;
        return ui::modifiers{
            .shift = has_modifier(m, key_modifiers::shift),
            .control = has_modifier(m, key_modifiers::control),
            .alt = has_modifier(m, key_modifiers::alt),
            .super = has_modifier(m, key_modifiers::super),
        };
    }

    ui::point to_point(const catalyst::math::vec2<std::int32_t> &p) noexcept
    {
        return ui::point{static_cast<float>(p.x()), static_cast<float>(p.y())};
    }

    const char *button_name(ui::pointer_button b) noexcept
    {
        switch (b)
        {
        case ui::pointer_button::left:
            return "left";
        case ui::pointer_button::right:
            return "right";
        case ui::pointer_button::middle:
            return "middle";
        case ui::pointer_button::extra1:
            return "extra1";
        case ui::pointer_button::extra2:
            return "extra2";
        default:
            return "none";
        }
    }

    // ---- the font -------------------------------------------------------------------------------

    /**
     * @brief Opens the first font that works: the one named on the command line, else a few faces most systems have.
     * @details The sample ships no font of its own, so it borrows one from the OS. A face that fails to parse is
     * logged and skipped, so a bad argument still leaves the sample running on a system face or on boxes.
     */
    bool open_font(ui_sample::ttf_text_provider &font, std::span<char *> args)
    {
        std::vector<std::filesystem::path> candidates;
        for (char *arg : args)
            candidates.emplace_back(arg);

        if (const char *windir = std::getenv("WINDIR"); windir != nullptr)
        {
            const std::filesystem::path fonts = std::filesystem::path(windir) / "Fonts";
            candidates.push_back(fonts / "segoeui.ttf");
            candidates.push_back(fonts / "arial.ttf");
            candidates.push_back(fonts / "calibri.ttf");
        }
        candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        candidates.emplace_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
        candidates.emplace_back("/usr/share/fonts/noto/NotoSans-Regular.ttf");
        candidates.emplace_back("/System/Library/Fonts/Supplemental/Arial.ttf");
        candidates.emplace_back("/System/Library/Fonts/Helvetica.ttc");

        for (const std::filesystem::path &path : candidates)
        {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec))
                continue;
            if (font.open(path))
            {
                logging::info<example_log>("font    {}", path.string());
                return true;
            }
            logging::warn<example_log>("font    {} skipped: {}", path.string(), font.error());
        }
        return false;
    }
} // namespace

int main(int argc, char **argv)
{
    // One console sink, and every line below reaches the terminal, coloured when the terminal understands colour.
    logging::default_logger().add_sink(logging::console_sink{});

    platform::window_desc desc;
    desc.title = "Catalyst - ui_sample";
    desc.width_px = ui::px(800.0f);
    desc.height_px = ui::px(450.0f);
    desc.visible = true;
    desc.resizable = true;

    platform::window w = platform::create_window(desc);
    if (!w)
    {
        logging::critical<example_log>("Failed to create window");
        return 1;
    }

    // One bus carries everything: window events from the platform layer, input events from the input context the
    // platform feeds, and the UI events the interaction state machine publishes back.
    events::bus bus;
    input::context in(bus);
    platform::set_input_feed(&in);
    platform::set_event_bus(&bus);

    // ---- rendering: device, swapchain, the UI renderer, and a ring of frames ----
    rendering::device_desc dd;
    dd.application_name = "catalyst_ui_sample";
    rendering::device dev = rendering::create_device(dd);
    if (!dev)
    {
        logging::critical<example_log>("Failed to create rendering device");
        return 1;
    }
    const rendering::device_info info = rendering::get_device_info(dev);
    logging::info<example_log>("Rendering backend: {} on {}", rendering::to_string(info.backend), info.adapter_name);

    const auto client_extent = [&]() -> rendering::extent2d
    {
        const platform::rect_px client = platform::client_rect_px(w);
        return {static_cast<std::uint32_t>(client.size().x()), static_cast<std::uint32_t>(client.size().y())};
    };

    rendering::swapchain_desc sd;
    sd.window = platform::get_native_handle(w);
    sd.extent = client_extent();
    sd.debug_name = "ui_sample swapchain";
    rendering::swapchain sc = rendering::create_swapchain(dev, sd);
    if (!sc)
    {
        logging::critical<example_log>("Failed to create swapchain");
        return 1;
    }
    sd = rendering::get_swapchain_desc(sc); // The backend may have adjusted extent, format or image count.

    constexpr std::uint32_t frames_in_flight = 3;

    // The renderer draws into whatever format the swapchain ended up with, and keeps one buffer set per frame slot.
    auto ui_renderer = ui::renderer::create(dev, {.color_format = sd.pixel_format,
                                                  .frames_in_flight = frames_in_flight,
                                                  .debug_name = "ui_sample"});
    if (!ui_renderer)
    {
        logging::critical<example_log>("Failed to create the UI renderer: {}", ui_renderer.error().message());
        return 1;
    }

    auto frames = rendering::frame_ring::create(dev, {.frames_in_flight = frames_in_flight, .debug_name = "ui_sample"});
    if (!frames)
    {
        logging::critical<example_log>("Failed to create the frame ring: {}", frames.error().message());
        return 1;
    }
    std::vector<rendering::command_list> lists; // One per slot, created the first time each slot comes round.
    const rendering::queue graphics = rendering::get_queue(dev);

    // ---- the font, the tree, and the interaction state machine over it ----
    // The provider is chosen before the tree is built because every text node keeps a pointer to it. The atlas it
    // fills lives on the CPU; `sync_atlas` below is what turns it into the texture the renderer samples.
    ui_sample::ttf_text_provider font;
    ui::null_text_provider boxes;
    const bool have_font = open_font(font, std::span<char *>(argv + 1, static_cast<std::size_t>(argc > 1 ? argc - 1 : 0)));
    if (!have_font)
        logging::warn<example_log>("font    none found; text draws as boxes. Pass a .ttf path as the first argument.");

    screen s;
    s.provider = have_font ? static_cast<ui::text_provider *>(&font) : &boxes;
    s.build();

    ui::interaction ux(s.tree, bus);
    ux.set_root(s.root);

    ui::render_batch batch;
    ui::opacity_mode compositing = ui::opacity_mode::group; // What a translucent node becomes; "G" flips it.

    // Lays the tree out to the swapchain's size at the window's DPI and paints it. `layout` clears the dirty flags,
    // so the loop calls this only when something marked the tree dirty since the last frame; the batch it fills is
    // drawn every frame regardless.
    const auto relayout = [&](std::string_view reason)
    {
        const ui::extent viewport{static_cast<float>(sd.extent.width), static_cast<float>(sd.extent.height)};
        const float scale = platform::dpi_scale(w);

        ui::layout(s.tree, s.root, ui::layout_params::for_viewport(viewport, scale));

        batch.clear();
        ui::batch_builder builder(batch, ui::rect::from_pos_size(ui::point{0.0f, 0.0f}, viewport));
        ui::paint_params params = ui::paint_params::for_viewport(viewport, scale);
        params.compositing = compositing;
        ui::paint(s.tree, s.root, builder, params);

        logging::info<example_log>(
            "layout  {:<8} viewport={}x{} scale={:.2f} nodes={} commands={} vertices={} layers={}", reason,
            sd.extent.width, sd.extent.height, scale, s.tree.node_count(), batch.commands.size(),
            batch.vertices.size(), batch.layers.size());
    };

    // ---- the glyph atlas as a texture ----
    // Painting asks the provider for glyphs, and a glyph it has not seen is rasterised into the atlas then. So after
    // every paint the atlas may have new pixels, and occasionally a new size. New pixels mean a new texture: the old
    // one may still be read by a frame in flight, so it is retired and destroyed only once the ring has moved that
    // many frames on. A new size means the batch just painted holds texture coordinates for the old size, which the
    // caller handles by painting again before the upload.
    rendering::texture atlas_texture;
    struct retired_texture
    {
        rendering::texture texture;
        std::uint64_t retired_at = 0; ///< `frame_ring::frame_index()` when it was replaced.
    };
    std::vector<retired_texture> retired;

    const auto sync_atlas = [&]()
    {
        if (!have_font || !font.take_dirty())
            return;

        rendering::texture_desc td;
        td.extent = {font.width(), font.height(), 1};
        td.pixel_format = rendering::format::r8_unorm; // Single channel: the renderer reads it as a coverage mask.
        td.usage = rendering::texture_usage::sampled;
        td.debug_name = "ui_sample glyph atlas";
        rendering::texture next = rendering::create_texture(dev, td, font.pixels());
        if (!next)
        {
            logging::warn<example_log>("atlas   could not create a {}x{} r8 texture", font.width(), font.height());
            return;
        }
        if (atlas_texture)
            retired.push_back({atlas_texture, frames->frame_index()});
        atlas_texture = next;
        if (!ui_renderer->register_texture(font.key(), atlas_texture))
            logging::warn<example_log>("atlas   the renderer refused the atlas texture");

        logging::info<example_log>("atlas   {}x{} glyphs={} dropped={}", font.width(), font.height(), font.cached(),
                                   font.dropped());
    };

    // Lays out, paints, and brings the atlas up to date, painting again if the atlas grew under the first paint.
    const auto refresh = [&](std::string_view reason)
    {
        relayout(reason);
        for (int attempt = 0; attempt < 4 && font.take_resized(); ++attempt)
        {
            s.tree.mark_dirty(s.root);
            relayout("atlas");
        }
        sync_atlas();
    };

    const auto set_status = [&]()
    {
        s.status->text = std::string("hover: ") + s.name_of(ux.hovered()) + "   focus: " + s.name_of(ux.focused());
        s.tree.mark_dirty(s.status_node);
    };

    bool running = true;
    int clicks = 0;
    std::string dirty_reason = "initial";

    // ---- window events ----
    const auto sub_close = bus.add_listener<platform::window_close_requested_event>(
        [&](const platform::window_close_requested_event &) { running = false; });

    const auto sub_dpi = bus.add_listener<platform::window_dpi_changed_event>(
        [&](const platform::window_dpi_changed_event &)
        {
            s.tree.mark_dirty(s.root);
            dirty_reason = "dpi";
        });

    // ---- input events -> interaction ----
    const auto sub_move = bus.add_listener<input::mouse_move_event>(
        [&](const input::mouse_move_event &e) { ux.pointer_moved(to_point(e.position_px), to_ui(e.modifiers)); });

    const auto sub_button = bus.add_listener<input::mouse_button_event>(
        [&](const input::mouse_button_event &e)
        {
            if (e.action == input::button_action::press)
                ux.pointer_pressed(to_ui(e.button), to_point(e.position_px), to_ui(e.modifiers));
            else if (e.action == input::button_action::release)
                ux.pointer_released(to_ui(e.button), to_point(e.position_px), to_ui(e.modifiers));
        });

    const auto sub_wheel = bus.add_listener<input::mouse_wheel_event>(
        [&](const input::mouse_wheel_event &e)
        { ux.wheel(ui::point{e.delta.x(), e.delta.y()}, to_point(e.position_px), to_ui(e.modifiers)); });

    const auto sub_key = bus.add_listener<input::key_event>(
        [&](const input::key_event &e)
        {
            const auto key = static_cast<std::uint32_t>(e.code);
            if (e.action == input::button_action::release)
                ux.key_released(key, to_ui(e.modifiers));
            else
                ux.key_pressed(key, to_ui(e.modifiers), e.action == input::button_action::repeat);
        });

    const auto sub_text = bus.add_listener<input::text_input_event>(
        [&](const input::text_input_event &e)
        {
            for (const char32_t cp : e.text())
                ux.text_input(cp);
        });

    // ---- ui events -> the application ----
    const auto sub_enter = bus.add_listener<ui::pointer_enter_event>(
        [&](const ui::pointer_enter_event &e)
        {
            if (button *b = s.button_of(e.target))
                s.tree.mutable_style(b->node).background = b->hover;
            set_status();
            dirty_reason = "hover";
        });

    const auto sub_leave = bus.add_listener<ui::pointer_leave_event>(
        [&](const ui::pointer_leave_event &e)
        {
            if (button *b = s.button_of(e.target))
                s.tree.mutable_style(b->node).background = b->fill;
            set_status();
            dirty_reason = "hover";
        });

    const auto sub_click = bus.add_listener<ui::click_event>(
        [&](const ui::click_event &e)
        {
            logging::info<example_log>("click   {:<6} on {} at ({:.0f}, {:.0f}) local ({:.0f}, {:.0f})",
                                       button_name(e.button), s.name_of(e.target), e.position.x(), e.position.y(),
                                       e.local.x(), e.local.y());

            const button *b = s.button_of(e.target);
            if (b == nullptr || e.button != ui::pointer_button::left)
                return;

            if (std::string_view(b->name) == "Increment")
                ++clicks;
            else if (std::string_view(b->name) == "Reset")
                clicks = 0;
            else if (std::string_view(b->name) == "Quit")
                running = false;

            // The label changed, so the node it measures must be laid out again. mark_dirty walks up to the root,
            // which is what the loop checks.
            s.counter->text = "Clicks: " + std::to_string(clicks);
            s.tree.mark_dirty(s.counter_node);
            dirty_reason = "click";
        });

    const auto sub_focus_gained = bus.add_listener<ui::focus_gained_event>(
        [&](const ui::focus_gained_event &e)
        {
            logging::info<example_log>("focus   {} (was {})", s.name_of(e.target), s.name_of(e.previous));
            set_status();
            dirty_reason = "focus";
        });

    const auto sub_focus_lost = bus.add_listener<ui::focus_lost_event>(
        [&](const ui::focus_lost_event &)
        {
            set_status();
            dirty_reason = "focus";
        });

    const auto sub_key_down = bus.add_listener<ui::key_down_event>(
        [&](const ui::key_down_event &e)
        {
            if (!ui::is_null(e.target) && !e.repeat)
                logging::info<example_log>("key     {} delivered to {}",
                                           input::key_name(static_cast<input::key_code>(e.key)), s.name_of(e.target));
        });

    logging::info<example_log>("UI sample: {} nodes. Move over and click the buttons; resize the window; Escape quits.",
                               s.tree.node_count());

    while (running && platform::is_valid(w))
    {
        // Order matters: clear this frame's edges, let the window fill them (which drives the listeners above), poll
        // what has no window, then evaluate. Only then does the tree know whether it needs a new layout.
        in.new_frame();
        platform::pump_events();
        in.poll();
        in.update();

        if (in.state().was_key_pressed(input::key_code::escape))
            running = false;
        if (!running)
            break;

        // Flip the toast between group opacity (a layer the renderer composites) and per-node alpha scaling.
        if (in.state().was_key_pressed(input::key_code::g))
        {
            const bool group = compositing != ui::opacity_mode::group;
            compositing = group ? ui::opacity_mode::group : ui::opacity_mode::multiply;
            s.toast_label->text = group ? "group opacity 0.6  (G toggles)" : "multiply opacity 0.6  (G toggles)";
            s.tree.mark_dirty(s.toast_node);
            dirty_reason = "opacity";
        }

        // A minimised window has no client area: nothing to lay out against or draw into, so wait it out.
        const rendering::extent2d client = client_extent();
        if (client.width == 0 || client.height == 0)
        {
            std::this_thread::sleep_for(16ms);
            continue;
        }

        // Follow the window: the swapchain matches the client area, and the tree is laid out to the swapchain, so
        // both change together. A resize the platform reported through the window is caught here as well.
        if (client != sd.extent && rendering::resize_swapchain(sc, client))
        {
            sd = rendering::get_swapchain_desc(sc);
            s.tree.mark_dirty(s.root);
            dirty_reason = "resized";
        }

        if (s.tree.is_dirty(s.root))
            refresh(dirty_reason);

        // ---- draw the batch ----
        auto f = frames->begin(); // Waits for the frame three back, which is what makes this slot's buffers free.
        if (!f)
        {
            logging::error<example_log>("frame_ring: {}", f.error().message());
            break;
        }

        // An atlas replaced `frames_in_flight` frames ago was last read by a frame `begin` has now waited for.
        std::erase_if(retired,
                      [&](retired_texture &r)
                      {
                          if (f->index() < r.retired_at + frames_in_flight)
                              return false;
                          rendering::destroy_texture(r.texture);
                          return true;
                      });

        while (lists.size() <= f->slot())
            lists.push_back(rendering::create_command_list(f->pool(), "ui_sample frame"));
        const rendering::command_list cl = lists[f->slot()];

        rendering::texture back_buffer = rendering::acquire_next_image(sc);
        if (!back_buffer)
        {
            // Out of date: the next iteration's client-size check recreates it. Nothing was submitted this frame.
            f->end();
            std::this_thread::sleep_for(16ms);
            continue;
        }

        const rendering::color_attachment color{
            .target = back_buffer,
            .load = rendering::load_op::clear,
            .store = rendering::store_op::store,
            .clear = {palette::background.r, palette::background.g, palette::background.b, 1.0f},
        };

        rendering::begin_recording(cl);
        // Layers (group opacity) are drawn offscreen before the swapchain pass; the batch is uploaded here too.
        if (!ui_renderer->prepare(cl, batch, sd.extent, f->slot()))
            logging::warn<example_log>("ui renderer: could not prepare this frame's batch");
        rendering::begin_render_pass(cl, {.color_attachments = std::span{&color, 1}, .debug_name = "ui"});
        if (!ui_renderer->render(cl, batch, sd.extent, f->slot()))
            logging::warn<example_log>("ui renderer: could not draw this frame's batch");
        rendering::end_render_pass(cl);
        rendering::end_recording(cl);

        const auto done = rendering::submit(graphics, cl);
        if (!done)
        {
            logging::error<example_log>("submit failed: {}", done.error().message());
            f->end();
            if (rendering::is_fatal(done.error().code))
                break;
            continue;
        }

        rendering::present(sc);
        f->end(*done);
        rendering::pump(dev); // Retires what the GPU has finished with, including buffers the renderer outgrew.
    }

    // Teardown in dependency order: nothing may be in flight, the pools go before the device, and so does the renderer.
    rendering::wait_idle(dev);
    for (rendering::command_list &list : lists)
        rendering::destroy_command_list(list);
    *frames = rendering::frame_ring{};
    ui_renderer->destroy(); // The renderer does not own the atlas; it goes after the last draw that sampled it.
    for (retired_texture &r : retired)
        rendering::destroy_texture(r.texture);
    if (atlas_texture)
        rendering::destroy_texture(atlas_texture);
    rendering::destroy_swapchain(sc);
    rendering::destroy_device(dev);

    platform::set_event_bus(nullptr);
    platform::set_input_feed(nullptr);
    platform::destroy_window(w);
    return 0;
}
