/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The backend-agnostic draw list the UI module produces, and the builder that fills it.
 */

#pragma once

#include <catalyst/ui/color.hpp>
#include <catalyst/ui/geometry.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace catalyst::ui
{

    /**
     * @struct vertex
     * @brief One UI vertex: a position in pixels, a texture coordinate and a packed colour.
     * @details Twenty bytes, laid out so a renderer can hand the array straight to a vertex buffer:
     * two floats, two floats, one `uint32`. The colour is packed as `color::to_rgba8` packs it, red
     * in the low byte, with straight (not premultiplied) alpha; the renderer's blend state is
     * `blend_alpha`.
     */
    struct vertex
    {
        /** @brief Position in pixels, y down, origin at the top-left of the viewport. */
        point position{};
        /** @brief Texture coordinate. Ignored by untextured commands. */
        point uv{};
        /** @brief Packed RGBA8, red in the least significant byte. */
        std::uint32_t color = 0xFFFFFFFFu;
    };

    /** @brief The index type of a `render_batch`. 32-bit because a text-heavy screen passes 65k vertices easily. */
    using index = std::uint32_t;

    /**
     * @typedef texture_key
     * @brief Names a texture to a renderer. The UI module never looks inside it.
     * @details A `std::uint64_t` rather than a `rendering::texture` so that `catalyst_ui` links
     * nothing from the rendering module and a text provider can hand out atlas ids without agreeing
     * on a handle type with the renderer bridge. Zero means "no texture": the command is solid
     * colour.
     */
    using texture_key = std::uint64_t;

    /** @brief The key for a solid-colour command. */
    inline constexpr texture_key no_texture = 0;

    /**
     * @typedef layer_id
     * @brief Indexes `render_batch::layers`.
     */
    using layer_id = std::uint32_t;

    /** @brief The layer id of the batch itself: a command drawn straight into the target. */
    inline constexpr layer_id no_layer = 0xFFFF'FFFFu;

    /**
     * @struct draw_command
     * @brief A run of indices to draw with one clip rectangle and one texture.
     * @details Consecutive geometry that shares a clip, a texture and a layer is merged into one
     * command by `batch_builder`, so a screen of solid boxes is one draw. The clip is a rectangle in
     * pixels; the renderer sets it as the scissor. An infinite clip (the builder's default) means
     * "the whole viewport" and the renderer clamps it.
     */
    struct draw_command
    {
        /** @brief Offset of the first index in `render_batch::indices`. */
        std::uint32_t first_index = 0;
        /** @brief How many indices, always a multiple of three. */
        std::uint32_t index_count = 0;
        /** @brief The scissor rectangle in pixels. */
        rect clip{};
        /** @brief The texture to sample, or `no_texture`. */
        texture_key texture = no_texture;
        /**
         * @brief The innermost layer this command was recorded inside, or `no_layer`.
         * @details A renderer draws it into that layer's offscreen target rather than the batch's
         * own; see `layer`.
         */
        layer_id layer = no_layer;
    };

    /**
     * @struct layer
     * @brief A run of commands composited as one image at reduced opacity: group opacity.
     * @details Scaling every vertex's alpha by a node's opacity lets overlapping children show
     * through each other. A layer is the alternative: everything between `batch_builder::begin_layer`
     * and the matching `end_layer` is drawn at full opacity into an offscreen image the size of
     * `bounds`, and that image is then blended into the parent at `opacity`, where the parent is
     * the enclosing layer or, for `parent == no_layer`, the target itself.
     *
     * The command range `[first_command, end_command)` is the layer's whole subtree: commands
     * tagged with this layer's id are its own, and commands tagged with a descendant's id belong to
     * a nested layer, which the renderer composites at the point in the range where it starts.
     * Layers are numbered in the order they were begun, so a parent always has a smaller id than
     * its children and a renderer that walks the list backwards draws every child before its parent
     * needs it.
     */
    struct layer
    {
        /**
         * @brief The pixel rectangle composited, already intersected with the clip in effect when
         * the layer began. Nothing outside it survives, even if geometry inside the layer extends
         * further. May be empty, in which case the layer draws nothing.
         */
        rect bounds{};
        /** @brief The opacity the finished image is blended in at, in `[0, 1]`. */
        float opacity = 1.0f;
        /** @brief The enclosing layer, or `no_layer` for a top-level one. */
        layer_id parent = no_layer;
        /** @brief Index of the first command in `render_batch::commands` inside the layer. */
        std::uint32_t first_command = 0;
        /** @brief One past the last command inside the layer. */
        std::uint32_t end_command = 0;
    };

    /**
     * @struct render_batch
     * @brief Everything one frame of UI draws: vertices, indices, the commands that slice them, and
     * the layers that group commands.
     * @details Plain vectors, reused frame to frame: `clear` keeps the capacity, so a steady-state
     * frame allocates nothing. The renderer bridge uploads `vertices` and `indices` once and then
     * issues one indexed draw per command. A consumer that cannot draw layers should ask the paint
     * pass for `opacity_mode::multiply`, which never opens one, rather than ignore them: a layer's
     * commands drawn straight into the target come out at full opacity.
     */
    struct render_batch
    {
        std::vector<vertex> vertices;
        std::vector<index> indices;
        std::vector<draw_command> commands;
        std::vector<layer> layers;

        /** @brief Empties the batch without releasing its storage. */
        void clear() noexcept;

        /** @brief True when there is nothing to draw. */
        [[nodiscard]] bool empty() const noexcept { return commands.empty(); }
    };

    /**
     * @brief How many straight segments approximate a quarter circle of the given radius.
     * @details Enough that the largest chord error stays under about half a pixel, clamped to
     * `[1, 32]`; the same count is used for every corner of one shape so a border ring lines up
     * with its fill.
     */
    [[nodiscard]] std::uint32_t corner_segments(float radius) noexcept;

    /**
     * @class batch_builder
     * @brief Appends shapes to a `render_batch`, maintaining the clip stack and merging commands.
     * @details The builder culls whole shapes whose bounds miss the current clip, and it merges
     * geometry into the previous command whenever the clip and texture have not changed. It does
     * not clip geometry itself: the scissor does that, which is why clips are rectangles.
     *
     * Rounded corners are tessellated here rather than in a shader because a shader would tie the
     * batch format to one renderer, and the vertex cost is small: a corner costs at most 32
     * triangles, and most corners cost four.
     */
    class batch_builder
    {
    public:
        /**
         * @brief Wraps a batch. Geometry is appended after whatever the batch already holds.
         * @param batch The batch to append to. Must outlive the builder.
         * @param clip The initial clip. Defaults to an unbounded rectangle.
         */
        explicit batch_builder(render_batch &batch, const rect &clip = unbounded_clip()) noexcept;

        /** @brief The rectangle that stands for "no clip". */
        [[nodiscard]] static rect unbounded_clip() noexcept;

        // ---- state ------------------------------------------------------------------------------

        /**
         * @brief Intersects the current clip with `r` and makes the result current.
         * @details Nested clips only ever shrink, so a child that overflows its clipped parent cannot
         * paint outside the parent by clipping itself to something larger.
         */
        void push_clip(const rect &r);

        /** @brief Restores the clip from before the matching `push_clip`. Ignored at the bottom of the stack. */
        void pop_clip() noexcept;

        /** @brief The clip in effect. */
        [[nodiscard]] const rect &clip() const noexcept { return clip_stack_.back(); }

        /** @brief Sets the texture the following shapes sample. `no_texture` for solid colour. */
        void set_texture(texture_key key) noexcept { texture_ = key; }

        /** @brief The texture in effect. */
        [[nodiscard]] texture_key texture() const noexcept { return texture_; }

        /**
         * @brief Opens a layer: until the matching `end_layer`, shapes are drawn into an offscreen
         * image that is then composited at `opacity`.
         * @details The layer's bounds are `bounds` intersected with the current clip, and that
         * rectangle becomes the clip for everything inside, so shapes outside it are culled here as
         * they would be cut there. Layers nest; every `begin_layer` needs its `end_layer`, and a
         * clip pushed inside a layer must be popped inside it.
         * @param bounds The pixel rectangle the layer covers.
         * @param opacity The opacity the finished image is blended in at. Clamped to `[0, 1]`.
         * @return The new layer's id, an index into `render_batch::layers`.
         */
        layer_id begin_layer(const rect &bounds, float opacity);

        /** @brief Closes the innermost open layer. Ignored when no layer is open. */
        void end_layer() noexcept;

        /** @brief The innermost open layer, or `no_layer`. */
        [[nodiscard]] layer_id layer() const noexcept
        {
            return layer_stack_.empty() ? no_layer : layer_stack_.back();
        }

        // ---- shapes -----------------------------------------------------------------------------

        /** @brief A solid rectangle. Four vertices, two triangles. */
        void add_rect(const rect &r, const color &c);

        /**
         * @brief A rectangle with texture coordinates.
         * @param r The rectangle in pixels.
         * @param uv The texture coordinates at `r`'s corners: `uv.min` at the top-left, `uv.max` at the bottom-right.
         * @param c A colour the sampled texel is multiplied by. White leaves the texture as is.
         */
        void add_rect(const rect &r, const rect &uv, const color &c);

        /**
         * @brief A solid rectangle with rounded corners.
         * @details Radii are clamped as CSS clamps them: if two radii on one side sum to more than
         * that side, every radius is scaled down by the same factor. All-zero radii take the plain
         * `add_rect` path.
         * @param r The rectangle in pixels.
         * @param radii Corner radii in pixels.
         * @param c The fill colour.
         */
        void add_rounded_rect(const rect &r, const corners_px &radii, const color &c);

        /**
         * @brief A border ring inside a rectangle.
         * @details The ring runs from `r` inwards by `widths` on each side. Outer corners follow
         * `radii`; inner corners are the outer radius less the adjacent border width, clamped at
         * zero, which is a circular approximation of CSS's elliptical inner corner. A border whose
         * widths meet in the middle is drawn as a filled rounded rectangle.
         * @param r The outer rectangle in pixels.
         * @param widths The border width on each side, in pixels.
         * @param radii Outer corner radii in pixels.
         * @param c The border colour.
         */
        void add_border(const rect &r, const edges_px &widths, const corners_px &radii, const color &c);

        /** @brief A straight line segment of the given thickness, drawn as one quad. */
        void add_line(const point &a, const point &b, float thickness, const color &c);

        /**
         * @brief Raw triangles.
         * @details For content painters that build their own geometry: the vertices are copied and
         * the indices, which are relative to `vertices`, are rebased. Culled against the clip by
         * the vertices' bounds.
         * @param vertices The vertices to append.
         * @param indices Indices into `vertices`, three per triangle.
         */
        void add_triangles(std::span<const vertex> vertices, std::span<const index> indices);

        // ---- queries ----------------------------------------------------------------------------

        /** @brief The batch being built. */
        [[nodiscard]] render_batch &batch() noexcept { return batch_; }

        /** @brief The batch being built. */
        [[nodiscard]] const render_batch &batch() const noexcept { return batch_; }

    private:
        /** @brief The command to append to: the last one if its clip and texture match, otherwise a new one. */
        [[nodiscard]] draw_command &current_command();

        /** @brief True when a shape with these bounds cannot touch the clip and should be skipped. */
        [[nodiscard]] bool culled(const rect &bounds) const noexcept;

        /**
         * @brief Appends the outline of a rounded rectangle: `4 * points_per_corner` points,
         * clockwise from the leftmost point of the top-left corner.
         */
        void append_outline(const rect &r, const corners_px &radii, std::uint32_t points_per_corner);

        [[nodiscard]] index push_vertex(const point &p, const point &uv, std::uint32_t packed);
        void push_triangle(draw_command &cmd, index a, index b, index c);

        render_batch &batch_;
        std::vector<rect> clip_stack_;
        std::vector<layer_id> layer_stack_;
        texture_key texture_ = no_texture;
        std::vector<point> outline_;
    };

} // namespace catalyst::ui
