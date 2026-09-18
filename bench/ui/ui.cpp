/**
 * @file ui.cpp
 * @brief Frame-cost benchmarks for the catalyst::ui module.
 * @details Builds a dashboard-style screen (top bar, sidebar list, card grid, data table, toast
 * overlay) at three sizes, entirely from the public retained-tree API, and times the CPU half of a
 * frame: the layout pass, the paint pass into a `render_batch`, text layout on its own, hit
 * testing, and layout + paint together as one frame. The rendering module is never linked; what is
 * measured is the draw list the UI produces, which is what a renderer bridge would submit.
 *
 * Every timing is reported as best-of-N and median-of-N over repeated passes, because a single
 * pass on a laptop is noise. The batch statistics (vertices, indices, commands) show how much of the
 * tree survived clipping and how well consecutive geometry merged into single draws.
 * License: MIT (see LICENSE).
 */

#include <catalyst/ui/ui.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ui = catalyst::ui;

namespace
{
    // -------------------------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------------------------

    constexpr ui::extent viewport{1920.0f, 1080.0f};
    constexpr std::size_t reps = 15;         ///< Timed passes per measurement; best and median are reported.
    constexpr std::size_t hit_points = 4096; ///< Pointer positions per hit-test pass.

    /// One size of the dashboard. Node counts are roughly: sidebar 3/item, card 11, table row 6.
    struct scene_size
    {
        std::string_view name;
        std::size_t sidebar_items;
        std::size_t card_rows;
        std::size_t card_columns;
        std::size_t table_rows;
    };

    constexpr scene_size sizes[] = {
        {"small", 16, 4, 4, 30},
        {"medium", 40, 6, 8, 200},
        {"large", 100, 12, 12, 1000},
    };

    // -------------------------------------------------------------------------------------------
    // Deterministic generation
    // -------------------------------------------------------------------------------------------

    class rng
    {
    public:
        explicit rng(std::uint64_t seed) : state_(seed) {}

        std::uint64_t next() noexcept
        {
            state_ = state_ * 6364136223846793005ull + 1442695040888963407ull;
            return state_ >> 33;
        }

        std::uint64_t below(std::uint64_t n) noexcept { return next() % n; }

        float unit() noexcept { return static_cast<float>(below(1u << 24)) / static_cast<float>(1u << 24); }

    private:
        std::uint64_t state_;
    };

    constexpr std::string_view words[] = {
        "alpha",  "bravo",  "charlie", "delta", "echo",    "foxtrot",  "golf",    "hotel",
        "india",  "juliet", "kilo",    "lima",  "mike",    "november", "oscar",   "papa",
        "quebec", "romeo",  "sierra",  "tango", "uniform", "victor",   "whiskey", "xray",
    };

    std::string sentence(rng &r, std::size_t word_count)
    {
        std::string out;
        for (std::size_t i = 0; i < word_count; ++i)
        {
            if (i != 0)
                out += ' ';
            out += words[r.below(std::size(words))];
        }
        return out;
    }

    // -------------------------------------------------------------------------------------------
    // Palette
    // -------------------------------------------------------------------------------------------

    namespace palette
    {
        constexpr ui::color background = ui::color::from_rgba8(24, 26, 32);
        constexpr ui::color surface = ui::color::from_rgba8(34, 37, 46);
        constexpr ui::color surface_alt = ui::color::from_rgba8(40, 44, 54);
        constexpr ui::color border = ui::color::from_rgba8(60, 65, 78);
        constexpr ui::color accent = ui::color::from_rgba8(88, 140, 255);
        constexpr ui::color accent_soft = ui::color::from_rgba8(88, 140, 255, 64);
        constexpr ui::color text = ui::color::from_rgba8(230, 232, 238);
        constexpr ui::color text_dim = ui::color::from_rgba8(150, 156, 170);
        constexpr ui::color warning = ui::color::from_rgba8(255, 180, 60);
    } // namespace palette

    // -------------------------------------------------------------------------------------------
    // Scene
    // -------------------------------------------------------------------------------------------

    /**
     * @brief The null provider with a call counter, so the benchmark can report how many times layout
     * asks each text node for its size per pass. `metrics` is called once per `layout_text`.
     */
    class counting_provider final : public ui::text_provider
    {
    public:
        std::size_t measure_calls = 0;

        [[nodiscard]] ui::font_metrics metrics(ui::font_id font, float size_px) override
        {
            ++measure_calls;
            return inner_.metrics(font, size_px);
        }

        [[nodiscard]] bool glyph_of(ui::font_id font, float size_px, char32_t code_point, ui::glyph &out) override
        {
            return inner_.glyph_of(font, size_px, code_point, out);
        }

    private:
        ui::null_text_provider inner_;
    };

    /**
     * @brief One built dashboard: the tree, its root, and the text contents the tree points at.
     * @details `texts` is a deque and `provider` lives on the heap so that the pointers handed to the
     * tree and to each `text_content` stay valid when the scene is moved out of `build_scene`.
     */
    struct scene
    {
        ui::tree tree;
        ui::node root = ui::null_node;
        std::deque<ui::text_content> texts;
        std::unique_ptr<counting_provider> provider = std::make_unique<counting_provider>();
        std::vector<ui::node> leaves; ///< A sample of leaf nodes, for the "dirty one node" pass.
        std::vector<ui::node> text_nodes;

        ui::node text(ui::node parent, std::string s, ui::color c, bool wrap = false)
        {
            const ui::node n = tree.create_child(parent);
            auto &content = texts.emplace_back();
            content.text = std::move(s);
            content.style.color = c;
            content.style.wrap = wrap;
            content.provider = provider.get();
            content.attach(tree, n);
            if (wrap)
                tree.mutable_style(n).flex_shrink = 1.0f;
            text_nodes.push_back(n);
            return n;
        }
    };

    ui::node box(ui::tree &t, ui::node parent, ui::flex_direction direction = ui::flex_direction::row)
    {
        const ui::node n = t.create_child(parent);
        t.mutable_style(n).direction = direction;
        return n;
    }

    ui::node button(scene &s, ui::node parent, std::string label, ui::color fill, ui::color fg)
    {
        const ui::node n = box(s.tree, parent);
        auto &st = s.tree.mutable_style(n);
        st.height = ui::px(30.0f);
        st.padding = ui::edges_length::symmetric(ui::px(12.0f), ui::px(0.0f));
        st.background = fill;
        st.border = ui::edges_length::all(ui::px(1.0f));
        st.border_color = palette::border;
        st.border_radius = ui::corners_length::all(ui::px(6.0f));
        st.align_items = ui::align::center;
        s.text(n, std::move(label), fg);
        return n;
    }

    void build_top_bar(scene &s, rng &r)
    {
        const ui::node bar = box(s.tree, s.root);
        auto &st = s.tree.mutable_style(bar);
        st.height = ui::px(48.0f);
        st.padding = ui::edges_length::symmetric(ui::px(16.0f), ui::px(8.0f));
        st.background = palette::surface;
        st.border = ui::edges_length{.bottom = ui::px(1.0f)};
        st.border_color = palette::border;
        st.align_items = ui::align::center;
        st.set_gap(ui::px(8.0f));

        const ui::node logo = box(s.tree, bar);
        auto &logo_style = s.tree.mutable_style(logo);
        logo_style.width = ui::px(28.0f);
        logo_style.height = ui::px(28.0f);
        logo_style.background = palette::accent;
        logo_style.border_radius = ui::corners_length::all(ui::px(14.0f));
        logo_style.margin = ui::edges_length{.right = ui::px(12.0f)};

        for (int i = 0; i < 6; ++i)
            button(s, bar, sentence(r, 1), i == 0 ? palette::accent_soft : ui::colors::transparent, palette::text);

        const ui::node spacer = box(s.tree, bar);
        s.tree.mutable_style(spacer).flex_grow = 1.0f;

        for (int i = 0; i < 3; ++i)
        {
            const ui::node icon = box(s.tree, bar);
            auto &icon_style = s.tree.mutable_style(icon);
            icon_style.width = ui::px(32.0f);
            icon_style.height = ui::px(32.0f);
            icon_style.background = palette::surface_alt;
            icon_style.border_radius = ui::corners_length::all(ui::px(16.0f));
        }
    }

    ui::node build_body(scene &s)
    {
        const ui::node body = box(s.tree, s.root);
        s.tree.mutable_style(body).flex_grow = 1.0f;
        s.tree.mutable_style(body).min_height = ui::px(0.0f);
        return body;
    }

    void build_sidebar(scene &s, rng &r, ui::node body, std::size_t items)
    {
        const ui::node side = box(s.tree, body, ui::flex_direction::column);
        auto &st = s.tree.mutable_style(side);
        st.width = ui::px(260.0f);
        st.padding = ui::edges_length::all(ui::px(12.0f));
        st.background = palette::surface;
        st.border = ui::edges_length{.right = ui::px(1.0f)};
        st.border_color = palette::border;
        st.overflow = ui::overflow_mode::hidden;
        st.set_gap(ui::px(4.0f));

        for (std::size_t i = 0; i < items; ++i)
        {
            const ui::node item = box(s.tree, side);
            auto &item_style = s.tree.mutable_style(item);
            item_style.height = ui::px(36.0f);
            item_style.padding = ui::edges_length::symmetric(ui::px(10.0f), ui::px(0.0f));
            item_style.border_radius = ui::corners_length::all(ui::px(6.0f));
            item_style.align_items = ui::align::center;
            item_style.set_gap(ui::px(10.0f));
            if (i % 7 == 2)
                item_style.background = palette::accent_soft;

            const ui::node icon = box(s.tree, item);
            auto &icon_style = s.tree.mutable_style(icon);
            icon_style.width = ui::px(20.0f);
            icon_style.height = ui::px(20.0f);
            icon_style.background = palette::text_dim;
            icon_style.border_radius = ui::corners_length::all(ui::px(4.0f));

            s.text(item, sentence(r, 2), palette::text);
            s.leaves.push_back(icon);
        }
    }

    void build_card(scene &s, rng &r, ui::node row)
    {
        const ui::node card = box(s.tree, row, ui::flex_direction::column);
        auto &st = s.tree.mutable_style(card);
        st.flex_grow = 1.0f;
        st.flex_basis = ui::px(0.0f);
        st.min_width = ui::px(0.0f);
        st.padding = ui::edges_length::all(ui::px(12.0f));
        st.background = palette::surface;
        st.border = ui::edges_length::all(ui::px(1.0f));
        st.border_color = palette::border;
        st.border_radius = ui::corners_length::all(ui::px(8.0f));
        st.overflow = ui::overflow_mode::hidden;
        st.set_gap(ui::px(8.0f));

        const ui::node header = box(s.tree, card);
        s.tree.mutable_style(header).align_items = ui::align::center;
        s.tree.mutable_style(header).justify_content = ui::justify::space_between;
        s.text(header, sentence(r, 2), palette::text);

        const ui::node badge = box(s.tree, header);
        auto &badge_style = s.tree.mutable_style(badge);
        badge_style.padding = ui::edges_length::symmetric(ui::px(8.0f), ui::px(2.0f));
        badge_style.background = (r.below(3) == 0) ? palette::warning : palette::accent;
        badge_style.border_radius = ui::corners_length::all(ui::px(10.0f));
        badge_style.font_size = ui::px(11.0f);
        s.text(badge, sentence(r, 1), palette::background);

        const ui::node body_text = s.text(card, sentence(r, 12 + r.below(16)), palette::text_dim, true);
        s.tree.mutable_style(body_text).flex_grow = 1.0f;

        const ui::node footer = box(s.tree, card);
        s.tree.mutable_style(footer).set_gap(ui::px(8.0f));
        s.tree.mutable_style(footer).justify_content = ui::justify::end;
        button(s, footer, sentence(r, 1), ui::colors::transparent, palette::text_dim);
        s.leaves.push_back(button(s, footer, sentence(r, 1), palette::accent, palette::background));
    }

    ui::node build_main(scene &s, ui::node body)
    {
        const ui::node main = box(s.tree, body, ui::flex_direction::column);
        auto &st = s.tree.mutable_style(main);
        st.flex_grow = 1.0f;
        st.min_width = ui::px(0.0f);
        st.padding = ui::edges_length::all(ui::px(16.0f));
        st.overflow = ui::overflow_mode::hidden;
        st.set_gap(ui::px(16.0f));
        return main;
    }

    void build_cards(scene &s, rng &r, ui::node main, std::size_t rows, std::size_t columns)
    {
        const ui::node grid = box(s.tree, main, ui::flex_direction::column);
        s.tree.mutable_style(grid).set_gap(ui::px(12.0f));
        for (std::size_t y = 0; y < rows; ++y)
        {
            const ui::node row = box(s.tree, grid);
            s.tree.mutable_style(row).set_gap(ui::px(12.0f));
            s.tree.mutable_style(row).height = ui::px(140.0f);
            for (std::size_t x = 0; x < columns; ++x)
                build_card(s, r, row);
        }
    }

    void build_table(scene &s, rng &r, ui::node main, std::size_t rows)
    {
        constexpr std::size_t columns = 5;

        const ui::node table = box(s.tree, main, ui::flex_direction::column);
        auto &st = s.tree.mutable_style(table);
        st.flex_grow = 1.0f;
        st.min_height = ui::px(0.0f);
        st.background = palette::surface;
        st.border = ui::edges_length::all(ui::px(1.0f));
        st.border_color = palette::border;
        st.border_radius = ui::corners_length::all(ui::px(6.0f));
        st.overflow = ui::overflow_mode::hidden;

        const auto add_row = [&](bool header, std::size_t index)
        {
            const ui::node row = box(s.tree, table);
            auto &row_style = s.tree.mutable_style(row);
            row_style.height = ui::px(header ? 36.0f : 32.0f);
            row_style.flex_shrink = 0.0f;
            row_style.padding = ui::edges_length::symmetric(ui::px(12.0f), ui::px(0.0f));
            row_style.align_items = ui::align::center;
            if (header)
            {
                row_style.background = palette::surface_alt;
                row_style.border = ui::edges_length{.bottom = ui::px(1.0f)};
                row_style.border_color = palette::border;
            }
            else if (index % 2 == 1)
                row_style.background = palette::surface_alt.with_alpha(0.5f);

            for (std::size_t c = 0; c < columns; ++c)
            {
                const ui::node cell = box(s.tree, row);
                auto &cell_style = s.tree.mutable_style(cell);
                cell_style.flex_grow = (c == 1) ? 3.0f : 1.0f;
                cell_style.flex_basis = ui::px(0.0f);
                cell_style.min_width = ui::px(0.0f);
                cell_style.overflow = ui::overflow_mode::hidden;
                s.text(cell, header ? sentence(r, 1) : (c == 0 ? std::to_string(index) : sentence(r, 1 + r.below(3))),
                       header ? palette::text : palette::text_dim);
            }
        };

        add_row(true, 0);
        for (std::size_t i = 0; i < rows; ++i)
            add_row(false, i);
    }

    void build_overlay(scene &s, rng &r)
    {
        const ui::node toast = box(s.tree, s.root, ui::flex_direction::column);
        auto &st = s.tree.mutable_style(toast);
        st.position = ui::position_mode::absolute;
        st.inset =
            ui::edges_length{.left = ui::auto_(), .top = ui::auto_(), .right = ui::px(24.0f), .bottom = ui::px(24.0f)};
        st.width = ui::px(320.0f);
        st.padding = ui::edges_length::all(ui::px(14.0f));
        st.background = palette::surface_alt;
        st.border = ui::edges_length::all(ui::px(1.0f));
        st.border_color = palette::accent;
        st.border_radius = ui::corners_length::all(ui::px(8.0f));
        st.opacity = 0.92f;
        st.set_gap(ui::px(6.0f));
        s.text(toast, sentence(r, 3), palette::text);
        s.text(toast, sentence(r, 14), palette::text_dim, true);
    }

    scene build_scene(const scene_size &size)
    {
        rng r(0x5EEDu + size.sidebar_items);
        scene s;
        s.root = s.tree.create();
        auto &root_style = s.tree.mutable_style(s.root);
        root_style.direction = ui::flex_direction::column;
        root_style.width = ui::px(viewport.x());
        root_style.height = ui::px(viewport.y());
        root_style.background = palette::background;
        root_style.font_size = ui::px(14.0f);

        build_top_bar(s, r);
        const ui::node body = build_body(s);
        build_sidebar(s, r, body, size.sidebar_items);
        const ui::node main = build_main(s, body);
        build_cards(s, r, main, size.card_rows, size.card_columns);
        build_table(s, r, main, size.table_rows);
        build_overlay(s, r);
        return s;
    }

    // -------------------------------------------------------------------------------------------
    // Timing
    // -------------------------------------------------------------------------------------------

    struct sample
    {
        double best_ms = 0.0;
        double median_ms = 0.0;
        double worst_ms = 0.0;
    };

    /// Times `operation` `reps` times, after one untimed warm-up, and returns the spread in ms.
    template <typename Operation>
    sample time_ms(Operation &&operation)
    {
        using clock = std::chrono::steady_clock;
        std::invoke(operation);

        std::vector<double> samples;
        samples.reserve(reps);
        for (std::size_t i = 0; i < reps; ++i)
        {
            const auto start = clock::now();
            std::invoke(operation);
            samples.push_back(std::chrono::duration<double, std::milli>(clock::now() - start).count());
        }
        std::sort(samples.begin(), samples.end());
        return sample{samples.front(), samples[samples.size() / 2], samples.back()};
    }

    void print_row(std::string_view label, const sample &s, std::string_view note = {})
    {
        std::cout << "  " << std::left << std::setw(26) << label << std::right << std::fixed << std::setprecision(3)
                  << std::setw(10) << s.best_ms << std::setw(10) << s.median_ms << std::setw(10) << s.worst_ms;
        if (!note.empty())
            std::cout << "   " << note;
        std::cout << '\n';
    }

    std::string batch_note(const ui::render_batch &batch)
    {
        char buffer[128];
        std::snprintf(buffer, sizeof buffer, "%zu verts, %zu idx, %zu cmds", batch.vertices.size(),
                      batch.indices.size(), batch.commands.size());
        return buffer;
    }

    std::size_t leaf_bytes(const scene &s)
    {
        std::size_t total = 0;
        for (const auto &t : s.texts)
            total += t.text.size();
        return total;
    }

    void run_scene(const scene_size &size)
    {
        using clock = std::chrono::steady_clock;

        const auto build_start = clock::now();
        scene s = build_scene(size);
        const double build_ms = std::chrono::duration<double, std::milli>(clock::now() - build_start).count();

        const auto lp = ui::layout_params::for_viewport(viewport);
        const auto pp = ui::paint_params::for_viewport(viewport);

        std::cout << "scene: " << size.name << "  (" << s.tree.node_count() << " nodes, " << s.texts.size()
                  << " text nodes, " << leaf_bytes(s) << " text bytes)\n"
                  << "  build:                    " << std::fixed << std::setprecision(3) << build_ms << " ms\n"
                  << "  " << std::left << std::setw(26) << "pass" << std::right << std::setw(10) << "best"
                  << std::setw(10) << "median" << std::setw(10) << "worst" << "   (ms)\n";

        // Layout of a fully dirty tree: every node's style was just written. The note reports how
        // many times each text node was measured in one pass: the layout engine's amplification.
        const sample layout_full = time_ms(
            [&]
            {
                s.tree.mark_dirty(s.root);
                ui::layout(s.tree, s.root, lp);
            });
        s.provider->measure_calls = 0;
        ui::layout(s.tree, s.root, lp);
        char layout_note[96];
        std::snprintf(layout_note, sizeof layout_note, "%.1f measures per text node",
                      static_cast<double>(s.provider->measure_calls) / static_cast<double>(s.texts.size()));
        print_row("layout (all dirty)", layout_full, layout_note);

        // Layout when nothing changed. Today this costs the same as a full pass; when dirty-driven
        // relayout lands this row is the one that should collapse.
        const sample layout_clean = time_ms([&] { ui::layout(s.tree, s.root, lp); });
        print_row("layout (clean)", layout_clean);

        // Layout after a single leaf changes, the shape of most per-frame invalidation.
        std::size_t leaf_cursor = 0;
        const sample layout_one = time_ms(
            [&]
            {
                const ui::node n = s.leaves[leaf_cursor++ % s.leaves.size()];
                s.tree.mutable_style(n).width = ui::px(20.0f + static_cast<float>(leaf_cursor % 5));
                ui::layout(s.tree, s.root, lp);
            });
        print_row("layout (one leaf dirty)", layout_one);

        // Paint into a batch that is cleared and refilled each frame, the steady-state shape.
        ui::render_batch batch;
        const sample paint_reuse = time_ms(
            [&]
            {
                batch.clear();
                ui::batch_builder builder(batch, ui::rect::from_pos_size({}, viewport));
                ui::paint(s.tree, s.root, builder, pp);
            });
        print_row("paint (batch reused)", paint_reuse, batch_note(batch));

        // Paint into a fresh batch, so allocation growth is included.
        const sample paint_fresh = time_ms(
            [&]
            {
                ui::render_batch fresh;
                ui::batch_builder builder(fresh, ui::rect::from_pos_size({}, viewport));
                ui::paint(s.tree, s.root, builder, pp);
            });
        print_row("paint (fresh batch)", paint_fresh);

        // Text layout on its own, every string at its laid-out width, so the share of layout and
        // paint that is text can be read off directly.
        ui::text_layout scratch;
        const sample text_only = time_ms(
            [&]
            {
                std::size_t i = 0;
                for (const ui::node n : s.text_nodes)
                {
                    const auto &content = s.texts[i++];
                    const auto &result = s.tree.layout_of(n);
                    const float width = content.style.wrap ? result.content_box().width() : 1e30f;
                    ui::layout_text(*s.provider, content.text, content.style, result.font_px, width, scratch);
                }
            });
        print_row("text layout (all strings)", text_only);

        // Hit testing at pseudo-random viewport positions.
        std::vector<ui::point> points;
        points.reserve(hit_points);
        rng pr(1234);
        for (std::size_t i = 0; i < hit_points; ++i)
            points.push_back(ui::point{pr.unit() * viewport.x(), pr.unit() * viewport.y()});

        std::size_t hits = 0;
        const sample hit = time_ms(
            [&]
            {
                hits = 0;
                for (const auto &p : points)
                    hits += ui::is_null(ui::hit_test(s.tree, s.root, p)) ? 0u : 1u;
            });
        char hit_note[96];
        std::snprintf(hit_note, sizeof hit_note, "%.2f us/test, %zu/%zu hit", hit.median_ms * 1000.0 / hit_points, hits,
                      hit_points);
        print_row("hit test (4096 points)", hit, hit_note);

        // A whole CPU frame: relayout and repaint, the cost a renderer bridge would pay per frame.
        const sample frame = time_ms(
            [&]
            {
                s.tree.mark_dirty(s.root);
                ui::layout(s.tree, s.root, lp);
                batch.clear();
                ui::batch_builder builder(batch, ui::rect::from_pos_size({}, viewport));
                ui::paint(s.tree, s.root, builder, pp);
            });
        char frame_note[64];
        std::snprintf(frame_note, sizeof frame_note, "%.0f fps at median", 1000.0 / frame.median_ms);
        print_row("frame (layout + paint)", frame, frame_note);

        std::cout << '\n';
    }
} // namespace

namespace
{
    /// Prints every node's border box after one layout, in tree order, so two builds of the engine can be diffed.
    void dump_boxes(const ui::tree &t, ui::node n, int depth)
    {
        const auto &r = t.layout_of(n);
        std::printf("%*s%u %.3f %.3f %.3f %.3f%s", depth * 2, "", n.index, r.position.x(), r.position.y(), r.size.x(),
                    r.size.y(), r.hidden ? " hidden" : "");
        if (const ui::measure_fn measure = t.measure_of(n))
        {
            // What the content measures at the node's final width: the value a consistent layout's
            // basis for this node was derived from.
            ui::measure_input mi{};
            mi.available_width_px = r.content_box().width();
            mi.available_height_px = r.content_box().height();
            mi.context.font_px = r.font_px;
            const ui::extent at_width = measure(mi, t.measure_user(n));
            std::printf("  content@%.3f=%.3fx%.3f", mi.available_width_px, at_width.x(), at_width.y());
        }
        std::printf("\n");
        for (const ui::node c : t.children_of(n))
            dump_boxes(t, c, depth + 1);
    }

    void dump_scenes()
    {
        for (const auto &size : sizes)
        {
            scene s = build_scene(size);
            ui::layout(s.tree, s.root, ui::layout_params::for_viewport(viewport));
            std::printf("scene %s\n", size.name.data());
            dump_boxes(s.tree, s.root, 0);
        }
    }
} // namespace

int main(int argc, char **argv)
{
    if (argc > 1 && std::string_view(argv[1]) == "--dump")
    {
        dump_scenes();
        return 0;
    }

    std::cout << "catalyst::ui benchmark, viewport " << viewport.x() << "x" << viewport.y() << ", " << reps
              << " reps per pass\n\n";
    for (const auto &size : sizes)
        run_scene(size);
    return 0;
}
