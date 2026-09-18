/**
 * @file layout.cpp
 * @brief Implements the single-line flexbox layout engine declared in layout.hpp.
 * @details The pass is recursive and works in border-box space throughout. `layout_node` sizes one
 * node given the space its parent offers and any size that parent forces on it; `layout_children`
 * runs the flex algorithm for one container and places its in-flow children relative to that
 * container's padding box. Positions stay parent-relative until `apply_offsets` walks the finished
 * tree once and turns them absolute.
 *
 * Every node is visited in one of two modes. A *measure* visit answers "how big would you be under
 * this constraint" and writes nothing into the tree; its answers are memoised per pass by node and
 * constraint, so asking twice costs once. A *perform* visit writes the node's final box and places
 * its children. The flex algorithm needs each child's hypothetical size before it can hand out the
 * final one, so a container measures its children first and performs them once afterwards; without
 * the memo and the measure/perform split that "first ask, then place" pattern lays out a subtree
 * about 2^depth times.
 * License: MIT (see LICENSE).
 */

#include <catalyst/ui/layout.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace catalyst::ui
{

    namespace
    {

        /**
         * @brief Tolerance for "the size did not really change", in pixels. Below this a relayout is skipped.
         */
        constexpr float k_epsilon = 1e-4f;

        /**
         * @brief The upper bound used when a node has no `max_width`/`max_height`.
         */
        constexpr float k_no_max = std::numeric_limits<float>::infinity();

        [[nodiscard]] bool is_row(flex_direction d) noexcept
        {
            return d == flex_direction::row || d == flex_direction::row_reverse;
        }

        [[nodiscard]] bool is_reversed(flex_direction d) noexcept
        {
            return d == flex_direction::row_reverse || d == flex_direction::column_reverse;
        }

        [[nodiscard]] axis main_axis_of(flex_direction d) noexcept
        {
            return is_row(d) ? axis::x : axis::y;
        }
        [[nodiscard]] axis cross_axis_of(flex_direction d) noexcept
        {
            return is_row(d) ? axis::y : axis::x;
        }

        [[nodiscard]] float get_axis(const extent &e, axis a) noexcept
        {
            return (a == axis::x) ? e.x() : e.y();
        }

        void set_axis(extent &e, axis a, float v) noexcept
        {
            if (a == axis::x)
                e.x() = v;
            else
                e.y() = v;
        }

        /**
         * @brief What one node contributes to its parent's flex line, produced by `layout_node`.
         * @details The bounds are resolved border-box sizes so the flex algorithm can clamp without
         * re-resolving the child's measurements against the child's own context.
         */
        struct measured
        {
            extent size{};
            edges_px margin{};
            edges_px surround{}; ///< Border plus padding, for turning a content-box basis into a border-box one.
            float min_w = 0.0f;
            float max_w = k_no_max;
            float min_h = 0.0f;
            float max_h = k_no_max;
        };

        [[nodiscard]] float min_along(const measured &m, axis a) noexcept
        {
            return (a == axis::x) ? m.min_w : m.min_h;
        }
        [[nodiscard]] float max_along(const measured &m, axis a) noexcept
        {
            return (a == axis::x) ? m.max_w : m.max_h;
        }

        /**
         * @brief The space a parent offers a child, plus any border-box size it forces on it.
         * @details A forced size is how the flex algorithm feeds a grown, shrunk or stretched result
         * back into a child so the child can lay its own contents out against the size it ended up with.
         */
        struct outer_constraint
        {
            float avail_w = 0.0f;
            float avail_h = 0.0f;
            bool avail_w_bounded = false;
            bool avail_h_bounded = false;
            float forced_w = 0.0f;
            float forced_h = 0.0f;
            bool forced_w_def = false;
            bool forced_h_def = false;

            [[nodiscard]] bool operator==(const outer_constraint &) const noexcept = default;

            void force(axis a, float v) noexcept
            {
                if (a == axis::x)
                {
                    forced_w = v;
                    forced_w_def = true;
                }
                else
                {
                    forced_h = v;
                    forced_h_def = true;
                }
            }
        };

        /**
         * @brief Everything `layout_node` derives from a node's style before it looks at content.
         * @details Split out so a container can resolve a child's margin and bounds without measuring
         * the child, which is what lets it skip measuring children whose size is already decided.
         */
        struct frame
        {
            resolve_context ctx{};
            float font_px = 16.0f;
            edges_px margin{};
            edges_px border{};
            edges_px padding{};
            edges_px surround{};
            float min_w = 0.0f;
            float max_w = k_no_max;
            float min_h = 0.0f;
            float max_h = k_no_max;
            float spec_w = 0.0f;
            float spec_h = 0.0f;
            bool spec_w_def = false;
            bool spec_h_def = false;
            float inner_w = 0.0f;
            float inner_h = 0.0f;
            bool inner_w_bounded = false;
            bool inner_h_bounded = false;

            [[nodiscard]] float clamp_w(float v) const noexcept { return std::clamp(std::max(0.0f, v), min_w, max_w); }
            [[nodiscard]] float clamp_h(float v) const noexcept { return std::clamp(std::max(0.0f, v), min_h, max_h); }

            [[nodiscard]] measured as_measured(const extent &size) const noexcept
            {
                measured m{};
                m.size = size;
                m.margin = margin;
                m.surround = surround;
                m.min_w = min_w;
                m.max_w = max_w;
                m.min_h = min_h;
                m.max_h = max_h;
                return m;
            }
        };

        [[nodiscard]] frame resolve_frame(const style &s, const resolve_context &parent_ctx, const outer_constraint &oc)
        {
            frame f{};
            f.ctx = parent_ctx;
            f.ctx.parent_width_px = oc.avail_w;
            f.ctx.parent_height_px = oc.avail_h;

            // `font_size` resolves against the parent's font, then becomes the font every `em` in this
            // node's own properties and in its children resolves against.
            f.font_px =
                s.font_size.is_auto ? parent_ctx.font_px : resolve_or(s.font_size, axis::x, f.ctx, parent_ctx.font_px);
            f.ctx.font_px = f.font_px;

            f.margin = resolve(s.margin, f.ctx);
            f.border = resolve(s.border, f.ctx);
            f.padding = resolve(s.padding, f.ctx);
            f.surround = f.border + f.padding;

            const bool content_box = (s.sizing == box_sizing::content_box);
            const auto to_border = [&](float v, axis a) noexcept { return content_box ? v + f.surround.along(a) : v; };

            const auto bound = [&](const length &l, axis a, float fallback) noexcept
            {
                if (l.is_auto)
                    return fallback;
                return std::max(0.0f, to_border(resolve_or(l, a, f.ctx, 0.0f), a));
            };

            f.min_w = bound(s.min_width, axis::x, 0.0f);
            f.max_w = bound(s.max_width, axis::x, k_no_max);
            f.min_h = bound(s.min_height, axis::y, 0.0f);
            f.max_h = bound(s.max_height, axis::y, k_no_max);

            if (oc.forced_w_def)
            {
                f.spec_w = f.clamp_w(oc.forced_w);
                f.spec_w_def = true;
            }
            else if (!s.width.is_auto)
            {
                f.spec_w = f.clamp_w(to_border(resolve_or(s.width, axis::x, f.ctx, 0.0f), axis::x));
                f.spec_w_def = true;
            }

            if (oc.forced_h_def)
            {
                f.spec_h = f.clamp_h(oc.forced_h);
                f.spec_h_def = true;
            }
            else if (!s.height.is_auto)
            {
                f.spec_h = f.clamp_h(to_border(resolve_or(s.height, axis::y, f.ctx, 0.0f), axis::y));
                f.spec_h_def = true;
            }

            f.inner_w = std::max(0.0f, f.spec_w_def ? f.spec_w - f.surround.horizontal()
                                                    : oc.avail_w - f.margin.horizontal() - f.surround.horizontal());
            f.inner_h = std::max(0.0f, f.spec_h_def ? f.spec_h - f.surround.vertical()
                                                    : oc.avail_h - f.margin.vertical() - f.surround.vertical());

            f.inner_w_bounded = f.spec_w_def || oc.avail_w_bounded;
            f.inner_h_bounded = f.spec_h_def || oc.avail_h_bounded;
            return f;
        }

        /**
         * @brief Zeroes the layout result of a `display_mode::none` subtree so stale boxes cannot be painted or hit.
         */
        void mark_hidden(tree &t, node n)
        {
            layout_result &r = t.mutable_layout(n);
            r = layout_result{};
            r.hidden = true;

            const std::span<const node> kids = t.children_of(n);
            const std::vector<node> children(kids.begin(), kids.end());
            for (const node &c : children)
                mark_hidden(t, c);
        }

        /**
         * @brief Returns how far `position_mode::relative` shifts a node on one axis, without affecting siblings.
         */
        [[nodiscard]] float relative_offset(const edges_length &inset, axis a, const resolve_context &ctx) noexcept
        {
            const length &lead = (a == axis::x) ? inset.left : inset.top;
            const length &trail = (a == axis::x) ? inset.right : inset.bottom;

            if (!lead.is_auto)
                return resolve_or(lead, a, ctx, 0.0f);
            if (!trail.is_auto)
                return -resolve_or(trail, a, ctx, 0.0f);

            return 0.0f;
        }

        /**
         * @brief One in-flow child of a flex container while its main-axis size is being resolved.
         */
        struct flex_item
        {
            node n{};
            measured m{};
            float base = 0.0f;
            float target = 0.0f;
            bool frozen = false;
            bool stretch = false;    ///< Auto cross size under `align::stretch`: the line decides its cross size.
            bool main_known = false; ///< `m.size` on the main axis is the child's real hypothetical size.
        };

        /**
         * @brief Memo of measure visits for one pass: what each node answered for each constraint.
         * @details A node is usually asked under two or three distinct constraints per pass (the offer
         * from its parent's measure visit, the same offer from the parent's perform visit, and a forced
         * size), so entries are kept in short per-node chains through one flat vector. Keys compare
         * floats exactly: the same inputs produce the same bits, and a near miss is simply a miss.
         */
        class measure_cache
        {
        public:
            struct key
            {
                outer_constraint oc{};
                float font_px = 0.0f;

                [[nodiscard]] bool operator==(const key &) const noexcept = default;
            };

            [[nodiscard]] const measured *find(node n, const key &k) const noexcept
            {
                if (n.index >= heads_.size())
                    return nullptr;
                for (std::int32_t i = heads_[n.index]; i >= 0; i = entries_[static_cast<std::size_t>(i)].next)
                {
                    const entry &e = entries_[static_cast<std::size_t>(i)];
                    if (e.k == k)
                        return &e.m;
                }
                return nullptr;
            }

            void store(node n, const key &k, const measured &m)
            {
                if (n.index >= heads_.size())
                    heads_.resize(static_cast<std::size_t>(n.index) + 1u, -1);
                entries_.push_back(entry{k, m, heads_[n.index]});
                heads_[n.index] = static_cast<std::int32_t>(entries_.size() - 1u);
            }

        private:
            struct entry
            {
                key k{};
                measured m{};
                std::int32_t next = -1;
            };

            std::vector<std::int32_t> heads_;
            std::vector<entry> entries_;
        };

        /**
         * @brief One layout pass over one tree: the recursion plus the memo it shares.
         */
        class pass
        {
        public:
            explicit pass(tree &t) noexcept : t_(t) {}

            /**
             * @brief Sizes one node under a constraint and, when `perform` is set, writes its box and places its
             * subtree.
             * @details A measure visit (`perform == false`) touches nothing in the tree and is memoised. A
             * perform visit always runs, because its job is the side effect.
             */
            measured layout_node(node n, const resolve_context &parent_ctx, const outer_constraint &oc, bool perform)
            {
                const style &s = t_.style_of(n);

                if (s.display == display_mode::none)
                {
                    mark_hidden(t_, n);
                    return measured{};
                }

                const measure_cache::key key{oc, parent_ctx.font_px};
                if (!perform)
                {
                    if (const measured *hit = cache_.find(n, key))
                        return *hit;
                }

                const frame f = resolve_frame(s, parent_ctx, oc);

                extent content{};
                const measure_fn measure = t_.measure_of(n);
                const bool has_children = t_.child_count(n) > 0u;

                if (measure != nullptr)
                {
                    measure_input mi{};
                    mi.available_width_px = f.inner_w;
                    mi.available_height_px = f.inner_h;
                    mi.width_definite = f.spec_w_def;
                    mi.height_definite = f.spec_h_def;
                    mi.context = f.ctx;

                    content = measure(mi, t_.measure_user(n));
                    content.x() = std::max(0.0f, content.x());
                    content.y() = std::max(0.0f, content.y());
                }
                else if (has_children)
                {
                    content = layout_children(n, f.ctx, f.inner_w, f.inner_h, f.spec_w_def, f.spec_h_def,
                                              f.inner_w_bounded, f.inner_h_bounded, f.padding, perform);
                }

                const float final_w = f.clamp_w(f.spec_w_def ? f.spec_w : content.x() + f.surround.horizontal());
                const float final_h = f.clamp_h(f.spec_h_def ? f.spec_h : content.y() + f.surround.vertical());

                const measured result = f.as_measured(extent{final_w, final_h});

                if (!perform)
                {
                    cache_.store(n, key, result);
                    return result;
                }

                // Min/max clamping can move the content box after the children were placed against it. That
                // only happens when a bound actually bit, so the second pass is rare and never cascades: it
                // runs with definite sizes, which its own children then agree with.
                if (measure == nullptr && has_children)
                {
                    const float fit_w = std::max(0.0f, final_w - f.surround.horizontal());
                    const float fit_h = std::max(0.0f, final_h - f.surround.vertical());

                    if (std::fabs(fit_w - content.x()) > k_epsilon || std::fabs(fit_h - content.y()) > k_epsilon)
                        layout_children(n, f.ctx, fit_w, fit_h, true, true, true, true, f.padding, true);
                }

                layout_absolutes(n, f.ctx, std::max(0.0f, final_w - f.border.horizontal()),
                                 std::max(0.0f, final_h - f.border.vertical()));

                layout_result &out = t_.mutable_layout(n);
                out.size = result.size;
                out.margin = f.margin;
                out.border = f.border;
                out.padding = f.padding;
                out.font_px = f.font_px;
                out.laid_out = true;
                out.hidden = false;

                cache_.store(n, key, result);
                return result;
            }

        private:
            /**
             * @brief Runs the flex algorithm for one container and, when `perform` is set, places its in-flow
             * children.
             * @param inner_w The content width offered to the children.
             * @param inner_h The content height offered to the children.
             * @param inner_w_def Whether `inner_w` is the container's final content width rather than an offer.
             * @param inner_h_def Whether `inner_h` is the container's final content height rather than an offer.
             * @param inner_w_bounded Whether `inner_w` is a known upper bound at all.
             * @param inner_h_bounded Whether `inner_h` is a known upper bound at all.
             * @param padding The container's resolved padding, added to child positions so they end up relative
             * to the padding box.
             * @param perform Whether to give the children their final sizes and positions. A measure visit stops
             * once the line's size is known, because nothing after that changes the container's own size.
             * @return The container's used content size.
             */
            extent layout_children(node parent, const resolve_context &ctx, float inner_w, float inner_h,
                                   bool inner_w_def, bool inner_h_def, bool inner_w_bounded, bool inner_h_bounded,
                                   const edges_px &padding, bool perform)
            {
                const style &ps = t_.style_of(parent);
                const flex_direction dir = ps.direction;
                const axis main = main_axis_of(dir);
                const axis cross = cross_axis_of(dir);

                const float inner_main = (main == axis::x) ? inner_w : inner_h;
                const float inner_cross = (cross == axis::x) ? inner_w : inner_h;
                const bool main_def = (main == axis::x) ? inner_w_def : inner_h_def;
                const bool cross_def = (cross == axis::x) ? inner_w_def : inner_h_def;
                const bool main_bounded = (main == axis::x) ? inner_w_bounded : inner_h_bounded;
                const bool cross_bounded = (cross == axis::x) ? inner_w_bounded : inner_h_bounded;

                const length &gap_length = (main == axis::x) ? ps.column_gap : ps.row_gap;
                const float gap = std::max(0.0f, resolve_or(gap_length, main, ctx, 0.0f));

                const std::span<const node> kids = t_.children_of(parent);
                const std::vector<node> children(kids.begin(), kids.end());

                std::vector<flex_item> items;
                items.reserve(children.size());

                outer_constraint offer{};
                offer.avail_w = inner_w;
                offer.avail_h = inner_h;
                offer.avail_w_bounded = inner_w_bounded;
                offer.avail_h_bounded = inner_h_bounded;

                // Hypothetical sizes. A child is only measured when its style leaves a size to content:
                // an explicit or flex-basis main size needs no content, and neither does the cross size of
                // a child that will be stretched to a definite line, which is the common column-of-rows
                // case. When the cross size is already decided the child is measured at that size, so the
                // perform visit below asks its children the very same questions and hits the memo.
                for (const node &c : children)
                {
                    const style &cs = t_.style_of(c);

                    if (cs.display == display_mode::none)
                    {
                        mark_hidden(t_, c);
                        continue;
                    }

                    if (cs.position == position_mode::absolute)
                        continue; // Placed later, once the container's own size is known.

                    frame f = resolve_frame(cs, ctx, offer);

                    const align self = (cs.align_self == align::automatic) ? ps.align_items : cs.align_self;
                    const bool cross_auto = (cross == axis::x) ? !f.spec_w_def : !f.spec_h_def;
                    const bool main_spec = (main == axis::x) ? f.spec_w_def : f.spec_h_def;

                    flex_item item{};
                    item.n = c;
                    item.stretch = (self == align::stretch) && cross_auto;

                    outer_constraint oc = offer;
                    if (item.stretch && cross_def)
                        oc.force(cross, std::max(0.0f, inner_cross - f.margin.along(cross)));

                    const bool main_known = main_spec || !cs.flex_basis.is_auto;
                    const bool cross_known = !cross_auto || (item.stretch && cross_def);

                    if (main_known && cross_known)
                    {
                        if (oc.forced_w_def || oc.forced_h_def)
                            f = resolve_frame(cs, ctx, oc);
                        item.m = f.as_measured(extent{f.spec_w, f.spec_h});
                        item.main_known = main_spec;
                    }
                    else
                    {
                        item.m = layout_node(c, ctx, oc, false);
                        item.main_known = true;
                    }

                    float base = get_axis(item.m.size, main);
                    if (!cs.flex_basis.is_auto)
                    {
                        base = resolve_or(cs.flex_basis, main, ctx, base);
                        if (cs.sizing == box_sizing::content_box)
                            base += item.m.surround.along(main);
                    }

                    item.base = std::clamp(std::max(0.0f, base), min_along(item.m, main), max_along(item.m, main));
                    item.target = item.base;
                    items.push_back(item);
                }

                const std::size_t count = items.size();
                const float total_gap = (count > 1u) ? gap * static_cast<float>(count - 1u) : 0.0f;

                float sum_margin = 0.0f;
                float sum_base = 0.0f;
                for (const flex_item &it : items)
                {
                    sum_margin += it.m.margin.along(main);
                    sum_base += it.base;
                }

                const float fixed = total_gap + sum_margin;
                const float hypothetical_main = fixed + sum_base;

                float line_main = 0.0f;
                if (main_def)
                    line_main = inner_main;
                else if (main_bounded)
                    line_main = std::min(hypothetical_main, inner_main);
                else
                    line_main = hypothetical_main;

                // Resolve flexible lengths: grow or shrink towards the line size, freezing any item whose
                // min/max bounds reject the size it was given and redistributing what it could not take.
                const float space_for_items = line_main - fixed;
                const bool growing = sum_base < space_for_items;

                for (flex_item &it : items)
                {
                    const style &cs = t_.style_of(it.n);
                    const float factor = growing ? cs.flex_grow : cs.flex_shrink;
                    if (factor <= 0.0f)
                        it.frozen = true;
                }

                for (std::size_t pass = 0; pass <= count; ++pass)
                {
                    float frozen_total = 0.0f;
                    float unfrozen_base = 0.0f;
                    float weight_total = 0.0f;
                    std::size_t unfrozen = 0u;

                    for (const flex_item &it : items)
                    {
                        if (it.frozen)
                        {
                            frozen_total += it.target;
                            continue;
                        }

                        ++unfrozen;
                        unfrozen_base += it.base;

                        const style &cs = t_.style_of(it.n);
                        weight_total += growing ? cs.flex_grow : (cs.flex_shrink * it.base);
                    }

                    if (unfrozen == 0u)
                        break;

                    const float free_space = (space_for_items - frozen_total) - unfrozen_base;

                    bool violated = false;
                    for (flex_item &it : items)
                    {
                        if (it.frozen)
                            continue;

                        if (weight_total > 0.0f)
                        {
                            const style &cs = t_.style_of(it.n);
                            const float weight = growing ? cs.flex_grow : (cs.flex_shrink * it.base);
                            it.target = it.base + free_space * (weight / weight_total);
                        }
                        else
                        {
                            it.target = it.base;
                        }

                        const float clamped =
                            std::clamp(std::max(0.0f, it.target), min_along(it.m, main), max_along(it.m, main));
                        if (std::fabs(clamped - it.target) > k_epsilon)
                        {
                            it.target = clamped;
                            it.frozen = true;
                            violated = true;
                        }
                    }

                    if (!violated)
                        break;
                }

                float used_main = fixed;
                for (const flex_item &it : items)
                    used_main += it.target;

                if (!main_def)
                    line_main = main_bounded ? std::min(used_main, inner_main) : used_main;

                // The line's cross size comes from the hypothetical cross sizes; stretching items to it
                // afterwards does not change it.
                float content_cross = 0.0f;
                for (const flex_item &it : items)
                    content_cross = std::max(content_cross, get_axis(it.m.size, cross) + it.m.margin.along(cross));

                float line_cross = 0.0f;
                if (cross_def)
                    line_cross = inner_cross;
                else if (cross_bounded)
                    line_cross = std::min(content_cross, inner_cross);
                else
                    line_cross = content_cross;

                extent content{};
                set_axis(content, main, line_main);
                set_axis(content, cross, line_cross);

                if (!perform)
                    return content;

                // Give every item its final size. Each gets one perform visit, always, so its subtree is
                // laid out against exactly the box it was handed; a child measured earlier under the same
                // constraint has its answers in the memo, so the visit costs its own placement only.
                for (flex_item &it : items)
                {
                    outer_constraint oc = offer;
                    // A child that keeps its hypothetical main size is visited under the offer it was
                    // measured with, so its subtree's memo entries match; one whose size the line changed,
                    // or whose basis came from `flex_basis` rather than content, is told its size.
                    if (!it.main_known || std::fabs(it.target - get_axis(it.m.size, main)) > k_epsilon)
                        oc.force(main, it.target);
                    if (it.stretch)
                        oc.force(cross, std::max(0.0f, line_cross - it.m.margin.along(cross)));

                    it.m = layout_node(it.n, ctx, oc, true);
                }

                // Distribute whatever main-axis space the items did not consume.
                float consumed = fixed;
                for (const flex_item &it : items)
                    consumed += get_axis(it.m.size, main);

                const float leftover = std::max(0.0f, line_main - consumed);

                float cursor = 0.0f;
                float between = gap;

                switch (ps.justify_content)
                {
                case justify::start:
                    break;
                case justify::end:
                    cursor = leftover;
                    break;
                case justify::center:
                    cursor = leftover * 0.5f;
                    break;
                case justify::space_between:
                    if (count > 1u)
                        between = gap + leftover / static_cast<float>(count - 1u);
                    break;
                case justify::space_around:
                    if (count > 0u)
                    {
                        const float unit = leftover / static_cast<float>(count);
                        cursor = unit * 0.5f;
                        between = gap + unit;
                    }
                    break;
                case justify::space_evenly:
                    if (count > 0u)
                    {
                        const float unit = leftover / static_cast<float>(count + 1u);
                        cursor = unit;
                        between = gap + unit;
                    }
                    break;
                }

                const bool reversed = is_reversed(dir);

                for (std::size_t i = 0; i < count; ++i)
                {
                    flex_item &it = items[reversed ? (count - 1u - i) : i];

                    const style &cs = t_.style_of(it.n);
                    const align self = (cs.align_self == align::automatic) ? ps.align_items : cs.align_self;

                    const float item_main = get_axis(it.m.size, main);
                    const float item_cross = get_axis(it.m.size, cross);

                    const float pos_main = cursor + it.m.margin.start(main);
                    cursor += it.m.margin.along(main) + item_main + between;

                    float pos_cross = it.m.margin.start(cross);
                    if (self == align::end)
                        pos_cross = line_cross - it.m.margin.end(cross) - item_cross;
                    else if (self == align::center)
                        pos_cross =
                            (line_cross - (item_cross + it.m.margin.along(cross))) * 0.5f + it.m.margin.start(cross);

                    point p{};
                    if (main == axis::x)
                        p = point{pos_main, pos_cross};
                    else
                        p = point{pos_cross, pos_main};

                    p.x() += padding.left;
                    p.y() += padding.top;

                    if (cs.position == position_mode::relative)
                    {
                        p.x() += relative_offset(cs.inset, axis::x, ctx);
                        p.y() += relative_offset(cs.inset, axis::y, ctx);
                    }

                    t_.mutable_layout(it.n).position = p;
                }

                return content;
            }

            /**
             * @brief Places every absolutely positioned child of `parent` against `parent`'s padding box.
             * @param pb_w The width of the containing padding box.
             * @param pb_h The height of the containing padding box.
             */
            void layout_absolutes(node parent, const resolve_context &ctx, float pb_w, float pb_h)
            {
                const std::span<const node> kids = t_.children_of(parent);
                const std::vector<node> children(kids.begin(), kids.end());

                resolve_context abs_ctx = ctx;
                abs_ctx.parent_width_px = pb_w;
                abs_ctx.parent_height_px = pb_h;

                for (const node &c : children)
                {
                    const style &cs = t_.style_of(c);
                    if (cs.display == display_mode::none || cs.position != position_mode::absolute)
                        continue;

                    const bool has_left = !cs.inset.left.is_auto;
                    const bool has_right = !cs.inset.right.is_auto;
                    const bool has_top = !cs.inset.top.is_auto;
                    const bool has_bottom = !cs.inset.bottom.is_auto;

                    const float left = has_left ? resolve_or(cs.inset.left, axis::x, abs_ctx, 0.0f) : 0.0f;
                    const float right = has_right ? resolve_or(cs.inset.right, axis::x, abs_ctx, 0.0f) : 0.0f;
                    const float top = has_top ? resolve_or(cs.inset.top, axis::y, abs_ctx, 0.0f) : 0.0f;
                    const float bottom = has_bottom ? resolve_or(cs.inset.bottom, axis::y, abs_ctx, 0.0f) : 0.0f;

                    outer_constraint oc{};
                    oc.avail_w = pb_w;
                    oc.avail_h = pb_h;
                    oc.avail_w_bounded = true;
                    oc.avail_h_bounded = true;

                    // Opposing insets with an auto size pin both edges, which determines the size. The
                    // margins that enter that arithmetic come from the style alone, so no measure is needed
                    // before the one perform visit.
                    const frame f = resolve_frame(cs, abs_ctx, oc);
                    if (has_left && has_right && cs.width.is_auto)
                        oc.force(axis::x, std::max(0.0f, pb_w - left - right - f.margin.horizontal()));
                    if (has_top && has_bottom && cs.height.is_auto)
                        oc.force(axis::y, std::max(0.0f, pb_h - top - bottom - f.margin.vertical()));

                    const measured m = layout_node(c, abs_ctx, oc, true);

                    float x = m.margin.left;
                    if (has_left)
                        x = left + m.margin.left;
                    else if (has_right)
                        x = pb_w - right - m.margin.right - m.size.x();

                    float y = m.margin.top;
                    if (has_top)
                        y = top + m.margin.top;
                    else if (has_bottom)
                        y = pb_h - bottom - m.margin.bottom - m.size.y();

                    t_.mutable_layout(c).position = point{x, y};
                }
            }

            tree &t_;
            measure_cache cache_;
        };

        /**
         * @brief Converts parent-relative child positions into absolute ones in a single top-down walk,
         * and gathers each node's `subtree_bounds` on the way back up.
         */
        void apply_offsets(tree &t, node n, point absolute)
        {
            layout_result &r = t.mutable_layout(n);
            if (!r.laid_out || r.hidden)
                return;

            r.position = absolute;

            const point child_origin{absolute.x() + r.border.left, absolute.y() + r.border.top};
            const bool clips = t.style_of(n).overflow != overflow_mode::visible;
            const rect clip = r.padding_box();
            rect bounds = r.border_box();

            const std::span<const node> kids = t.children_of(n);
            const std::vector<node> children(kids.begin(), kids.end());

            for (const node &c : children)
            {
                const layout_result &cr = t.layout_of(c);
                if (!cr.laid_out || cr.hidden)
                    continue;

                apply_offsets(t, c, point{child_origin.x() + cr.position.x(), child_origin.y() + cr.position.y()});

                rect cb = t.layout_of(c).subtree_bounds;
                if (clips)
                {
                    cb.min = point{std::max(cb.min.x(), clip.min.x()), std::max(cb.min.y(), clip.min.y())};
                    cb.max = point{std::min(cb.max.x(), clip.max.x()), std::min(cb.max.y(), clip.max.y())};
                    if (cb.is_empty())
                        continue;
                }
                bounds.min = point{std::min(bounds.min.x(), cb.min.x()), std::min(bounds.min.y(), cb.min.y())};
                bounds.max = point{std::max(bounds.max.x(), cb.max.x()), std::max(bounds.max.y(), cb.max.y())};
            }

            t.mutable_layout(n).subtree_bounds = bounds;
        }

    } // namespace

    void layout(tree &t, node root, const layout_params &params) noexcept
    {
        if (!t.is_valid(root))
            return;

        outer_constraint oc{};
        oc.avail_w = params.available.x();
        oc.avail_h = params.available.y();
        oc.avail_w_bounded = params.width_definite;
        oc.avail_h_bounded = params.height_definite;

        pass p(t);
        const measured m = p.layout_node(root, params.context, oc, true);

        apply_offsets(t, root, point{params.origin.x() + m.margin.left, params.origin.y() + m.margin.top});

        t.clear_dirty(root);
    }

} // namespace catalyst::ui
