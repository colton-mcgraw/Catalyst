/*
 * @file main.cpp
 * @brief A small retained UI drawn in a real window through `ui::app`, the UI shell: a styled tree, real text, and
 * the events it publishes, with no window, device or frame-loop code of its own.
 * @details `ui::app` creates the window, the bus and input context, the device, swapchain and frame ring, and a
 * `ui::host` sized to the window; this file builds a header, a row of three buttons, a card of wrapped text, a status
 * line and a translucent toast under the host's root and reacts to what the tree publishes. The host lays the tree
 * out and paints it whenever something marks it dirty, forwards mouse and keyboard input into it, uploads the glyph
 * atlas whenever painting adds glyphs, and draws it with `prepare` before the swapchain pass (the toast's
 * group-opacity layer) and `render` inside it. The window keeps redrawing while its border is dragged, because the
 * app installs the platform's frame callback.
 *
 * Text is real: `font_provider.hpp` is a `ui::text_provider` over stb_truetype that rasterises glyphs on demand into
 * an `r8` coverage atlas and is its own `ui::glyph_atlas`, which is all the host needs to find, upload and register
 * it. The font comes from the command line (`catalyst_ui_sample path/to/font.ttf`) or from a short list of system
 * faces; with none found the host's `null_text_provider` draws boxes instead. `--wait` runs the app in
 * `loop_mode::wait`, drawing only when something changed and sleeping in between, as a tool would. Hover recolours a
 * button, clicking "Increment" or "Reset" rewrites the counter label, "G" flips the toast between group and per-node
 * opacity, "Quit" or Escape ends the loop.
 *
 * Every length in the tree is in `dp`, so the layout follows the monitor's DPI scale, and "+" / "-" / "0" set the
 * app's zoom, which multiplies that scale: the same tree, laid out and painted larger or smaller, text included. "P"
 * shows a profiler panel built from `app::timings()`: CPU time per phase of the frame (events, layout, paint, atlas
 * upload, the frame ring wait, acquire, record, submit, present) averaged over the last half second, with the batch
 * and atlas sizes behind them; a one-line summary also goes to the log every few seconds.
 * License: MIT (see LICENSE).
 */

// The MSVC CRT deprecates getenv in favour of _dupenv_s; the font search below reads WINDIR with it, the same way
// the logging terminal sink reads NO_COLOR and TERM, so the same opt-out applies. Must precede every include.
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <catalyst/events/bus.hpp>
#include <catalyst/input/input.hpp>
#include <catalyst/logging/logging.hpp>
#include <catalyst/platform/window.hpp>
#include <catalyst/rendering/rendering.hpp>
#include <catalyst/ui/app.hpp>
#include <catalyst/ui/ui.hpp>

#include "font_provider.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace platform = catalyst::platform;
namespace input = catalyst::input;
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
     * @brief The sample's tree, built under the host's root: the text the nodes point at, and the handles the event
     * handlers need to find their way back to a node.
     * @details `texts` is a deque because `text_content::attach` hands the tree a pointer to the content, and a deque
     * keeps its elements in place as more are added. The tree stores only pointers, so the content must outlive it.
     */
    struct screen
    {
        screen(ui::tree &t, ui::node r, ui::text_provider &p) : tree(t), root(r), provider(&p) {}

        ui::tree &tree;
        ui::node root = ui::null_node;
        ui::text_provider *provider = nullptr; ///< The font every text node is set with: the host's.
        std::deque<ui::text_content> texts;
        ui::text_content *counter = nullptr;
        ui::node counter_node = ui::null_node;
        ui::text_content *status = nullptr;
        ui::node status_node = ui::null_node;
        ui::node toast = ui::null_node;
        ui::text_content *toast_label = nullptr;
        ui::node toast_node = ui::null_node;
        ui::node profile_panel = ui::null_node;
        ui::text_content *profile_label = nullptr;
        ui::node profile_node = ui::null_node;
        std::vector<button> buttons;
        std::vector<std::pair<ui::node, const char *>> names;

        /**
         * @brief Creates a text leaf under `parent`.
         * @details The size goes on the node's `font_size` in `dp`, not on `text_style::size_px`: `size_px` is raw
         * pixels, which ignore both the DPI scale and the zoom. Zero inherits the parent's size, and the root sets
         * the default.
         */
        ui::node text(ui::node parent, std::string s, ui::color c, bool wrap = false, float size_dp = 0.0f)
        {
            const ui::node n = tree.create_child(parent);
            ui::text_content &content = texts.emplace_back();
            content.text = std::move(s);
            content.style.color = c;
            content.style.wrap = wrap;
            content.provider = provider;
            content.attach(tree, n);
            if (wrap)
                tree.mutable_style(n).flex_shrink = 1.0f;
            if (size_dp > 0.0f)
                tree.mutable_style(n).font_size = ui::dp(size_dp);
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
            st.height = ui::dp(32.0f);
            st.padding = ui::edges_length::symmetric(ui::dp(14.0f), ui::dp(0.0f));
            st.background = fill;
            st.border = ui::edges_length::all(ui::px(1.0f));
            st.border_color = palette::border;
            st.border_radius = ui::corners_length::all(ui::dp(6.0f));
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

        /**
         * @brief Builds the tree under the root.
         * @details The host made the root a column that fills the viewport with a 15 dp default text size; this
         * only dresses it. Sizes are in `dp`, which the host multiplies by the scale the app gives it (the monitor's
         * DPI scale times the zoom), so the whole screen grows and shrinks together. Borders stay one real pixel: a
         * hairline looks the same at every scale.
         */
        void build()
        {
            {
                ui::style &st = tree.mutable_style(root);
                st.background = palette::background;
                st.padding = ui::edges_length::all(ui::dp(16.0f));
                st.set_gap(ui::dp(12.0f));
            }
            names.emplace_back(root, "root");

            // Header: a title on the left, a hint on the right.
            const ui::node header = box(root);
            {
                ui::style &st = tree.mutable_style(header);
                st.height = ui::dp(48.0f);
                st.padding = ui::edges_length::symmetric(ui::dp(16.0f), ui::dp(0.0f));
                st.background = palette::surface;
                st.border_radius = ui::corners_length::all(ui::dp(8.0f));
                st.align_items = ui::align::center;
                st.justify_content = ui::justify::space_between;
            }
            names.emplace_back(header, "header");
            text(header, "Catalyst UI sample", palette::text, false, 20.0f);
            text(header, "+ / - zoom   0 reset   P profiler   G opacity   Esc quits", palette::text_dim);

            // Toolbar: three buttons. The gap is on the container, so the buttons carry no margins.
            const ui::node toolbar = box(root);
            tree.mutable_style(toolbar).set_gap(ui::dp(8.0f));
            names.emplace_back(toolbar, "toolbar");
            add_button(toolbar, "Increment", palette::accent, palette::accent_hover);
            add_button(toolbar, "Reset", palette::button, palette::button_hover);
            add_button(toolbar, "Quit", palette::danger, palette::danger_hover);

            // Card: grows to fill whatever height is left, clips its content, and wraps its body text to its width.
            const ui::node card = box(root, ui::flex_direction::column);
            {
                ui::style &st = tree.mutable_style(card);
                st.flex_grow = 1.0f;
                st.min_height = ui::dp(0.0f);
                st.padding = ui::edges_length::all(ui::dp(12.0f));
                st.background = palette::surface;
                st.border = ui::edges_length::all(ui::px(1.0f));
                st.border_color = palette::border;
                st.border_radius = ui::corners_length::all(ui::dp(8.0f));
                st.overflow = ui::overflow_mode::hidden;
                st.set_gap(ui::dp(8.0f));
            }
            names.emplace_back(card, "card");
            counter_node = text(card, "Clicks: 0", palette::text, false, 18.0f);
            counter = &texts.back();
            text(card,
                 "This window is a ui::app: the shell owns the window, the device and the frame loop, and its "
                 "ui::host lays this tree out whenever something changes, forwards the mouse and keyboard into it, "
                 "uploads the glyph atlas as painting fills it, and draws it every frame. Text is set from a TrueType "
                 "face by a small stb_truetype provider that is its own glyph atlas. Resize the window to see the "
                 "layout follow it while you drag, move over the buttons to see hover, and click them to see click "
                 "events routed through the bus. Every length is in dp, so + and - rescale the whole screen and the "
                 "glyphs are rasterised again at the new size.",
                 palette::text_dim, true);

            // Status line: what the interaction state machine currently thinks.
            const ui::node status_bar = box(root);
            tree.mutable_style(status_bar).height = ui::dp(24.0f);
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
                st.inset.right = ui::dp(24.0f);
                st.inset.bottom = ui::dp(48.0f);
                st.padding = ui::edges_length::all(ui::dp(10.0f));
                st.background = palette::surface;
                st.border = ui::edges_length::all(ui::px(1.0f));
                st.border_color = palette::border;
                st.border_radius = ui::corners_length::all(ui::dp(8.0f));
                st.opacity = 0.6f;
                st.pointer_events = ui::pointer_mode::none;
            }
            names.emplace_back(toast, "toast");
            const ui::node pill = box(toast);
            {
                ui::style &st = tree.mutable_style(pill);
                st.height = ui::dp(32.0f);
                st.padding = ui::edges_length::symmetric(ui::dp(12.0f), ui::dp(0.0f));
                st.background = palette::accent;
                st.border_radius = ui::corners_length::all(ui::dp(16.0f));
                st.align_items = ui::align::center;
            }
            toast_node = text(pill, "group opacity 0.6  (G toggles)", palette::text);
            toast_label = &texts.back();

            // Profiler: a panel pinned under the header on the right, over the card, hidden until "P". Its label is
            // one text node with newlines, rewritten a couple of times a second by the loop; that rewrite is itself
            // a layout and a paint, which the numbers it shows include. `display::none` takes it out of layout
            // entirely, so hidden it costs nothing.
            profile_panel = box(root, ui::flex_direction::column);
            {
                ui::style &st = tree.mutable_style(profile_panel);
                st.position = ui::position_mode::absolute;
                st.inset.right = ui::dp(24.0f);
                st.inset.top = ui::dp(84.0f);
                st.padding = ui::edges_length::all(ui::dp(10.0f));
                st.background = srgb(34, 37, 46, 235);
                st.border = ui::edges_length::all(ui::px(1.0f));
                st.border_color = palette::border;
                st.border_radius = ui::corners_length::all(ui::dp(8.0f));
                st.pointer_events = ui::pointer_mode::none;
                st.display = ui::display_mode::none;
            }
            names.emplace_back(profile_panel, "profiler");
            profile_node = text(profile_panel, "profiler: no frames yet", palette::text, false, 13.0f);
            profile_label = &texts.back();
        }
    };

    // ---- profiling ------------------------------------------------------------------------------

    /**
     * @brief The app's per-frame timings, summarised over a reporting window.
     * @details `add` folds one frame's `ui::frame_timings` in, and `report()` turns a window of frames into one
     * `summary`: the frame rate, the average and worst whole-frame time, and for every phase how many times it ran
     * and the average and worst time per run. Layout and paint run only on frames where something changed, so their
     * run counts say how often the tree was dirty, and their averages are per layout, not per frame. All of it is CPU
     * time: the GPU shows up only as the time `frame_ring::begin` spends waiting for the slot's previous frame, and
     * as the time `present` blocks.
     */
    class frame_profiler
    {
    public:
        using clock = std::chrono::steady_clock;

        enum class phase : std::size_t
        {
            events,  ///< Pumping the window, polling and updating input, and every listener those drive.
            layout,  ///< `ui::layout` over the whole tree.
            paint,   ///< `ui::paint` into the batch, repaints included.
            atlas,   ///< Uploading a changed glyph atlas as a new texture.
            wait,    ///< `frame_ring::begin`: blocked until the slot's previous frame has finished on the GPU.
            acquire, ///< `acquire_next_image`.
            record,  ///< Recording the command list, including the renderer's batch upload and layer passes.
            submit,  ///< `submit`.
            present, ///< `present`, ending the frame, and `pump`.
            count,
        };
        static constexpr std::size_t phase_count = static_cast<std::size_t>(phase::count);
        static constexpr std::array<const char *, phase_count> phase_names{
            "events", "layout", "paint", "atlas", "wait", "acquire", "record", "submit", "present"};

        /** @brief One phase over one window. Times are per run. */
        struct phase_summary
        {
            double average_ms = 0.0;
            double worst_ms = 0.0;
            std::uint32_t runs = 0;
        };

        /** @brief One window of frames. */
        struct summary
        {
            std::uint32_t frames = 0;
            double seconds = 0.0; ///< How long the window was.
            double fps = 0.0;
            double frame_average_ms = 0.0;
            double frame_worst_ms = 0.0;
            std::array<phase_summary, phase_count> phases{};
        };

        /** @brief Folds in one frame: what the app timed, and the whole frame as the loop saw it. */
        void add(const ui::frame_timings &t, clock::duration whole_frame) noexcept
        {
            frame_.add(whole_frame);
            add(phase::events, t.events);
            if (t.ui.laid_out)
                add(phase::layout, t.ui.layout);
            if (t.ui.paints > 0)
                add(phase::paint, t.ui.paint);
            if (t.ui.atlas_uploaded)
                add(phase::atlas, t.ui.atlas);
            if (t.drawn)
            {
                add(phase::wait, t.wait);
                add(phase::acquire, t.acquire);
                add(phase::record, t.record);
                add(phase::submit, t.submit);
                add(phase::present, t.present);
            }
        }

        /**
         * @brief Closes the window when `interval` has passed since the last one.
         * @return True when a window closed; `out` then describes it and the counters start over.
         */
        bool report(clock::duration interval, summary &out) noexcept
        {
            const clock::time_point now = clock::now();
            if (now - window_start_ < interval || frame_.runs == 0)
                return false;

            out.frames = frame_.runs;
            out.seconds = std::chrono::duration<double>(now - window_start_).count();
            out.fps = static_cast<double>(frame_.runs) / out.seconds;
            out.frame_average_ms = ms(frame_.total) / static_cast<double>(frame_.runs);
            out.frame_worst_ms = ms(frame_.worst);
            for (std::size_t i = 0; i < phase_count; ++i)
            {
                const bucket &b = phases_[i];
                out.phases[i] = phase_summary{
                    .average_ms = b.runs == 0 ? 0.0 : ms(b.total) / static_cast<double>(b.runs),
                    .worst_ms = ms(b.worst),
                    .runs = b.runs,
                };
            }

            frame_ = bucket{};
            phases_.fill(bucket{});
            window_start_ = now;
            return true;
        }

    private:
        struct bucket
        {
            clock::duration total{};
            clock::duration worst{};
            std::uint32_t runs = 0;

            void add(clock::duration elapsed) noexcept
            {
                total += elapsed;
                worst = std::max(worst, elapsed);
                ++runs;
            }
        };

        void add(phase p, clock::duration elapsed) noexcept { phases_[static_cast<std::size_t>(p)].add(elapsed); }

        [[nodiscard]] static double ms(clock::duration d) noexcept
        {
            return std::chrono::duration<double, std::milli>(d).count();
        }

        std::array<bucket, phase_count> phases_{};
        bucket frame_{};
        clock::time_point window_start_ = clock::now();
    };

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

    // `--wait` runs the app in loop_mode::wait: a frame only when something changed, the thread asleep in between.
    // Anything else on the command line is a font to try.
    bool wait_mode = false;
    std::vector<char *> font_args;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string_view(argv[i]) == "--wait")
            wait_mode = true;
        else
            font_args.push_back(argv[i]);
    }

    // The font is chosen before the app exists because the host takes it at creation, and every text node keeps a
    // pointer to it.
    ui_sample::ttf_text_provider font;
    const bool have_font = open_font(font, font_args);
    if (!have_font)
        logging::warn<example_log>("font    none found; text draws as boxes. Pass a .ttf path as the first argument.");

    ui::app_desc desc;
    desc.window.title = "Catalyst - ui_sample";
    // dp, so the window is this size in 96 DPI terms on every monitor: twice as many pixels at 200 % scale.
    desc.window.width_px = ui::dp(900.0f);
    desc.window.height_px = ui::dp(540.0f);
    desc.application_name = "catalyst_ui_sample";
    desc.frames_in_flight = 3;
    desc.clear_color = palette::background;
    desc.text = have_font ? &font : nullptr;
    desc.loop = wait_mode ? ui::loop_mode::wait : ui::loop_mode::poll;

    auto app = ui::app::create(desc);
    if (!app)
    {
        logging::critical<example_log>("Failed to start: {}: {}", ui::to_string(app.error().stage),
                                       app.error().message);
        return 1;
    }
    const rendering::device_info info = rendering::get_device_info(app->device());
    logging::info<example_log>("Rendering backend: {} on {}; {} loop", rendering::to_string(info.backend),
                               info.adapter_name, wait_mode ? "wait" : "poll");

    screen s(app->tree(), app->root(), app->text());
    s.build();

    ui::host &host = app->host();
    ui::interaction &ux = host.interaction();
    catalyst::events::bus &bus = app->bus();

    frame_profiler profiler;
    bool show_profile = false;
    constexpr auto report_interval = 500ms; // How often the panel is rewritten and the counters start over.
    constexpr int log_every = 10;           // Reports per summary line in the log: one every five seconds.
    int reports = 0;

    // The zoom multiplies the DPI scale, so every dp in the tree, text included, grows with it. "+" and "-" step it,
    // "0" puts it back.
    constexpr float zoom_step = 1.1f;
    constexpr float zoom_min = 0.5f;
    constexpr float zoom_max = 4.0f;

    const auto set_status = [&]()
    {
        s.status->text = std::string("hover: ") + s.name_of(ux.hovered()) + "   focus: " + s.name_of(ux.focused());
        s.tree.mark_dirty(s.status_node);
    };

    // The profiler panel's text: one line per phase, then what the batch and the atlas currently hold.
    const auto describe = [&](const frame_profiler::summary &st)
    {
        std::string text = std::format("frame    {:6.2f} ms avg  {:6.2f} ms worst  {:5.0f} fps  ({} frames)\n",
                                       st.frame_average_ms, st.frame_worst_ms, st.fps, st.frames);
        for (std::size_t i = 0; i < frame_profiler::phase_count; ++i)
        {
            const frame_profiler::phase_summary &p = st.phases[i];
            text += std::format("{:<8} {:6.2f} ms avg  {:6.2f} ms worst  x{}\n", frame_profiler::phase_names[i],
                                p.average_ms, p.worst_ms, p.runs);
        }
        const ui::render_batch &batch = host.batch();
        text += std::format("batch    {} nodes  {} commands  {} vertices  {} indices  {} layers\n", s.tree.node_count(),
                            batch.commands.size(), batch.vertices.size(), batch.indices.size(), batch.layers.size());
        if (have_font)
            text += std::format("atlas    {}x{}  {} glyphs  {} dropped\n", font.width(), font.height(), font.cached(),
                                font.dropped());
        const float dpi = platform::dpi_scale(app->window());
        text += std::format("scale    dpi {:.2f} x zoom {:.2f} = {:.2f}", dpi, app->zoom(), dpi * app->zoom());
        return text;
    };

    int clicks = 0;

    // ---- ui events -> the application ----
    // Input reaches the tree through the host's bridge; what comes back is UI events on the same bus.
    const auto sub_enter = bus.add_listener<ui::pointer_enter_event>(
        [&](const ui::pointer_enter_event &e)
        {
            if (button *b = s.button_of(e.target))
                s.tree.mutable_style(b->node).background = b->hover;
            set_status();
        });

    const auto sub_leave = bus.add_listener<ui::pointer_leave_event>(
        [&](const ui::pointer_leave_event &e)
        {
            if (button *b = s.button_of(e.target))
                s.tree.mutable_style(b->node).background = b->fill;
            set_status();
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
                app->request_quit();

            // The label changed, so the node it measures must be laid out again. mark_dirty walks up to the root,
            // which is what the host checks.
            s.counter->text = "Clicks: " + std::to_string(clicks);
            s.tree.mark_dirty(s.counter_node);
        });

    const auto sub_focus_gained = bus.add_listener<ui::focus_gained_event>(
        [&](const ui::focus_gained_event &e)
        {
            logging::info<example_log>("focus   {} (was {})", s.name_of(e.target), s.name_of(e.previous));
            set_status();
        });

    const auto sub_focus_lost =
        bus.add_listener<ui::focus_lost_event>([&](const ui::focus_lost_event &) { set_status(); });

    const auto sub_key_down = bus.add_listener<ui::key_down_event>(
        [&](const ui::key_down_event &e)
        {
            if (!ui::is_null(e.target) && !e.repeat)
                logging::info<example_log>("key     {} delivered to {}",
                                           input::key_name(static_cast<input::key_code>(e.key)), s.name_of(e.target));
        });

    logging::info<example_log>("UI sample: {} nodes. Move over and click the buttons; resize the window; + and - zoom, "
                               "0 resets; P shows the profiler; Escape quits.",
                               s.tree.node_count());

    for (;;)
    {
        const auto frame_start = frame_profiler::clock::now();
        if (!app->begin_frame())
            break;

        // Between begin_frame and end_frame the input state is this frame's.
        const auto &keys = app->input().state();
        if (keys.was_key_pressed(input::key_code::escape))
        {
            app->request_quit();
            break;
        }

        // Zoom in 10 % steps. The app hands the new scale to the host, which lays out again.
        {
            const float zoom = app->zoom();
            float next = zoom;
            if (keys.was_key_pressed(input::key_code::equal) || keys.was_key_pressed(input::key_code::keypad_plus))
                next = std::min(zoom * zoom_step, zoom_max);
            if (keys.was_key_pressed(input::key_code::minus) || keys.was_key_pressed(input::key_code::keypad_minus))
                next = std::max(zoom / zoom_step, zoom_min);
            if (keys.was_key_pressed(input::key_code::digit_0))
                next = 1.0f;
            if (next != zoom)
                app->set_zoom(next);
        }

        if (keys.was_key_pressed(input::key_code::p))
        {
            show_profile = !show_profile;
            s.tree.mutable_style(s.profile_panel).display =
                show_profile ? ui::display_mode::flex : ui::display_mode::none;
            s.tree.mark_dirty(s.profile_panel);
        }

        // Flip the toast between group opacity (a layer the renderer composites) and per-node alpha scaling.
        if (keys.was_key_pressed(input::key_code::g))
        {
            const bool group = host.compositing() != ui::opacity_mode::group;
            host.set_compositing(group ? ui::opacity_mode::group : ui::opacity_mode::multiply);
            s.toast_label->text = group ? "group opacity 0.6  (G toggles)" : "multiply opacity 0.6  (G toggles)";
            s.tree.mark_dirty(s.toast_node);
        }

        if (!app->end_frame())
        {
            logging::error<example_log>("drawing failed; stopping");
            break;
        }

        const ui::frame_timings &t = app->timings();
        profiler.add(t, frame_profiler::clock::now() - frame_start);
        if (t.ui.laid_out)
        {
            const ui::extent viewport = host.viewport();
            const ui::render_batch &batch = host.batch();
            logging::debug<example_log>(
                "layout  viewport={}x{} scale={:.2f} nodes={} commands={} vertices={} layers={}", viewport.x(),
                viewport.y(), host.scale(), s.tree.node_count(), batch.commands.size(), batch.vertices.size(),
                batch.layers.size());
        }
        if (t.ui.atlas_uploaded)
            logging::info<example_log>("atlas   {}x{} glyphs={} dropped={}", font.width(), font.height(), font.cached(),
                                       font.dropped());

        // ---- report ----
        // The panel is rewritten here, after this frame was drawn, so the rewrite is laid out on the next frame and
        // shows up in the window after that one.
        frame_profiler::summary stats;
        if (profiler.report(report_interval, stats))
        {
            if (show_profile)
            {
                s.profile_label->text = describe(stats);
                s.tree.mark_dirty(s.profile_node);
            }
            if (++reports % log_every == 0)
            {
                const auto &layout_phase = stats.phases[static_cast<std::size_t>(frame_profiler::phase::layout)];
                const auto &paint_phase = stats.phases[static_cast<std::size_t>(frame_profiler::phase::paint)];
                const auto &wait_phase = stats.phases[static_cast<std::size_t>(frame_profiler::phase::wait)];
                logging::info<example_log>(
                    "profile frame {:.2f} ms avg {:.2f} ms worst {:.0f} fps | layout {:.2f} ms x{} | paint {:.2f} ms "
                    "x{} | wait {:.2f} ms | commands={} vertices={}",
                    stats.frame_average_ms, stats.frame_worst_ms, stats.fps, layout_phase.average_ms, layout_phase.runs,
                    paint_phase.average_ms, paint_phase.runs, wait_phase.average_ms, host.batch().commands.size(),
                    host.batch().vertices.size());
            }
        }
    }

    // The app waits for the GPU and tears down the host, swapchain, device and window in order when it goes out of
    // scope. The listeners above are tokens into its bus and need nothing more.
    return 0;
}
