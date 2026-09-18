/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The renderer bridge: submits a `ui::render_batch` through `catalyst::rendering`.
 * @details `catalyst_ui` paints into a backend-agnostic `render_batch` and never touches a GPU. This
 * header is the other half: a `renderer` owns the one graphics pipeline UI geometry needs, a
 * sampler, a registry mapping the batch's `texture_key`s to real textures, and per frame in flight a
 * set of vertex and index buffers plus the offscreen images the batch's layers are drawn into.
 * `prepare` draws those layers before the caller's render pass and `render` uploads the batch and
 * records one scissored indexed draw per `draw_command` inside it, compositing each layer where it
 * belongs. It lives in its own target, `catalyst_ui_renderer`, so that `catalyst_ui` itself keeps
 * not linking the rendering module and stays testable without a device.
 */

#pragma once

#include <catalyst/rendering/buffer.hpp>
#include <catalyst/rendering/command.hpp>
#include <catalyst/rendering/device.hpp>
#include <catalyst/rendering/error.hpp>
#include <catalyst/rendering/pipeline.hpp>
#include <catalyst/rendering/texture.hpp>
#include <catalyst/rendering/types.hpp>
#include <catalyst/ui/batch.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

namespace catalyst::ui
{

    /**
     * @struct renderer_desc
     * @brief What a `renderer` needs to know up front.
     */
    struct renderer_desc
    {
        /**
         * @brief The pixel format of the colour attachment `render` will draw into, typically the
         * swapchain's `pixel_format` after `get_swapchain_desc` has reported what the backend chose.
         * @details Vertex colours are the module's linear values passed straight through, so an sRGB
         * attachment encodes them on write. Authors who think in sRGB bytes should decode before
         * building a `color`. Layer images use the same format, so they must be samplable in it,
         * which every 8-bit colour format is.
         */
        rendering::format color_format = rendering::format::bgra8_unorm_srgb;

        /**
         * @brief How many frames may be in flight, and so how many independent buffer sets to keep.
         * @details Match it to the `frame_ring` driving the loop and pass `frame::slot()` to `render`.
         * A frame's buffers are only rewritten when that slot comes round again, by which time the
         * ring has waited for the GPU to finish reading them.
         */
        std::uint32_t frames_in_flight = 2;

        /** @brief Vertices each slot's buffer starts out holding. Buffers grow, and never shrink. */
        std::uint32_t initial_vertices = 4096;

        /** @brief Indices each slot's buffer starts out holding. */
        std::uint32_t initial_indices = 8192;

        /** @brief Name for the pipeline and buffers in graphics debuggers. */
        const char *debug_name = nullptr;
    };

    /**
     * @class renderer
     * @brief Draws `render_batch` geometry with the rendering module.
     * @details Move-only. The device it was created on must outlive it; `destroy` (or the
     * destructor) returns the pipeline, buffers and images to that device.
     *
     * **Textures.** A `draw_command::texture` is a key the UI module made up; `register_texture`
     * tells the renderer which `rendering::texture` it names. What the shader does with the texture
     * follows its format: a single-channel `r8_unorm` image is a coverage mask, as a glyph atlas is,
     * and its red channel scales the vertex alpha; any other format is a straight-alpha colour image
     * multiplied by the vertex colour. A key that was never registered draws as solid colour, so a
     * batch never fails to draw for want of a texture. The renderer does not own registered
     * textures and never destroys them; unregister a texture before destroying it.
     *
     * **Layers.** A batch's `layer`s (group opacity, `opacity_mode::group`) each need an offscreen
     * image, drawn in a render pass of its own before the caller's. That is `prepare`: call it with
     * the recording command list *outside* any render pass, then begin the pass and call `render`,
     * which composites each layer at its place in the command order. A batch without layers needs
     * no `prepare`. Layer images are kept per frame slot like the buffers and grow to the largest
     * layer each index has held.
     */
    class renderer
    {
    public:
        /**
         * @brief Creates the pipeline, the sampler, and one buffer set per frame in flight.
         * @param dev The device to create on. Must stay valid for the renderer's lifetime.
         * @param desc The attachment format, frame count and initial capacities.
         * @return The renderer, or the error that stopped it: `invalid_argument` for a zero frame count
         * or capacity, `shader_invalid` or `pipeline_creation_failed` when the backend rejects the
         * embedded SPIR-V (a backend that does not consume SPIR-V reports `unsupported_operation`), and
         * `out_of_device_memory` when a buffer, sampler or image could not be made.
         */
        [[nodiscard]] static std::expected<renderer, rendering::error> create(const rendering::device &dev,
                                                                              const renderer_desc &desc = {});

        /** @brief An invalid renderer. `render` on it returns false. */
        renderer() noexcept = default;

        renderer(const renderer &) = delete;
        renderer &operator=(const renderer &) = delete;

        /** @brief Moves. The moved-from renderer is invalid and owns nothing. */
        renderer(renderer &&other) noexcept;

        /** @brief Move-assigns, destroying anything this renderer owned first. */
        renderer &operator=(renderer &&other) noexcept;

        /** @brief Destroys what the renderer owns. */
        ~renderer();

        // ---- textures ---------------------------------------------------------------------------

        /**
         * @brief Maps a batch texture key to a texture, replacing any earlier mapping of the key.
         * @param key The key commands and glyphs carry. `no_texture` cannot be mapped.
         * @param t A valid texture created with `texture_usage::sampled`. Not owned.
         * @return False, and nothing changes, when the key is `no_texture`, the texture is invalid,
         * or it was not created for sampling.
         */
        bool register_texture(texture_key key, const rendering::texture &t);

        /** @brief Forgets a key. Commands using it draw as solid colour from then on. */
        void unregister_texture(texture_key key) noexcept;

        /** @brief The texture a key maps to, or an invalid handle. */
        [[nodiscard]] rendering::texture texture_of(texture_key key) const noexcept;

        // ---- drawing ----------------------------------------------------------------------------

        /**
         * @brief Uploads a batch into one frame slot's buffers and draws its layers offscreen.
         * @details Must be called with `cl` recording and outside any render pass, before the pass
         * `render` will record into. Each visible layer, innermost first, gets a render pass into
         * that slot's image for it, drawn with the same pipeline and the layer's own commands, with
         * nested layers composited in. The following `render` on the same slot then uses those
         * images and does not upload again.
         *
         * Optional for a batch with no layers; required for one with them, or `render` refuses it.
         * @param cl The command list to record into.
         * @param batch What to draw. Its layers and commands must be well formed, as `batch_builder` leaves them.
         * @param viewport The size in pixels of the attachment `render` will draw into.
         * @param slot The frame slot, below `frames_in_flight()`.
         * @return False when the renderer is invalid, `slot` is out of range, the viewport is empty,
         * the batch's layers are malformed, or a buffer or image could not be made or written.
         */
        bool prepare(const rendering::command_list &cl, const render_batch &batch, rendering::extent2d viewport,
                     std::uint32_t slot = 0);

        /**
         * @brief Records a batch's draws into the current render pass.
         * @details The command list must be recording, inside a render pass whose colour attachment
         * has the format the renderer was created for. `viewport` is the attachment's size in
         * pixels; the batch's pixel coordinates map onto it with the origin at the top left, as
         * `paint` produced them. Unless `prepare` just ran for this slot, the batch is uploaded
         * here. The call records `set_pipeline`, the pixel-to-clip push constants, the buffers, the
         * sampler, and then per command a texture binding when it changes, a `set_scissor` clamped
         * to the viewport and one `draw_indexed`; a layer is one textured quad at its bounds.
         * Commands whose clip lies entirely outside the viewport are skipped.
         *
         * Buffers grow to fit and keep their size. Growing replaces that slot's buffer, which is safe
         * because the caller's frame ring has already waited for the last frame that read it.
         * @param cl The command list to record into.
         * @param batch What to draw. An empty batch records nothing and returns true.
         * @param viewport The attachment size in pixels.
         * @param slot The frame slot, below `frames_in_flight()`.
         * @return False when the renderer is invalid, `slot` is out of range, the viewport is empty,
         * the batch has layers but `prepare` did not run for it on this slot, or a buffer could not
         * be grown or written.
         */
        bool render(const rendering::command_list &cl, const render_batch &batch, rendering::extent2d viewport,
                    std::uint32_t slot = 0);

        /** @brief Destroys the pipeline, buffers and images, leaving the renderer invalid. Safe to call twice. */
        void destroy() noexcept;

        /** @brief True while the renderer owns a pipeline. */
        [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(pipeline_); }

        /** @brief True while the renderer owns a pipeline. */
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

        /** @brief The device the renderer was created on, or an invalid handle. */
        [[nodiscard]] rendering::device owner() const noexcept { return device_; }

        /** @brief The description the renderer was created with. */
        [[nodiscard]] const renderer_desc &desc() const noexcept { return desc_; }

        /** @brief How many frame slots there are. Zero for an invalid renderer. */
        [[nodiscard]] std::uint32_t frames_in_flight() const noexcept
        {
            return static_cast<std::uint32_t>(slots_.size());
        }

        /** @brief How many vertices a slot's buffer currently holds, or zero for a bad slot. */
        [[nodiscard]] std::uint32_t vertex_capacity(std::uint32_t slot) const noexcept;

        /** @brief How many indices a slot's buffer currently holds, or zero for a bad slot. */
        [[nodiscard]] std::uint32_t index_capacity(std::uint32_t slot) const noexcept;

        /**
         * @brief How many layer images a slot holds, or zero for a bad slot.
         * @details Grows to the most layers a batch prepared on the slot has had, and never shrinks;
         * an image is only made for a layer that was visible at least once.
         */
        [[nodiscard]] std::uint32_t layer_count(std::uint32_t slot) const noexcept;

    private:
        /** @brief An offscreen image for one layer index of one slot, at least as large as any layer it has held. */
        struct layer_target
        {
            rendering::texture texture{};
            rendering::extent2d capacity{};
        };

        /** @brief Where a layer of the batch being prepared lands, in whole pixels of the target. */
        struct layer_placement
        {
            std::int32_t x = 0;
            std::int32_t y = 0;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            bool visible = false;
        };

        struct slot_buffers
        {
            rendering::buffer vertices{};
            rendering::buffer indices{};
            std::uint32_t vertex_capacity = 0;
            std::uint32_t index_capacity = 0;
            std::vector<layer_target> layers;
            std::vector<layer_placement> placements;
            /** @brief `prepare` ran since the last `render`; the sizes below say for which batch. */
            bool prepared = false;
            std::size_t prepared_vertices = 0;
            std::size_t prepared_indices = 0;
            std::size_t prepared_commands = 0;
            std::size_t prepared_layers = 0;
        };

        struct texture_entry
        {
            texture_key key = no_texture;
            rendering::texture texture{};
            std::uint32_t mode = 0;
        };

        /** @brief The state `draw_range` carries between draws so bindings and constants change only when they must. */
        struct draw_state;

        [[nodiscard]] bool reserve(slot_buffers &s, std::uint32_t vertices, std::uint32_t indices);
        [[nodiscard]] bool upload(slot_buffers &s, const render_batch &batch, rendering::extent2d viewport);
        [[nodiscard]] bool ensure_layer_target(layer_target &target, rendering::extent2d needed);
        [[nodiscard]] const texture_entry *find_texture(texture_key key) const noexcept;
        void bind_common(const rendering::command_list &cl, const slot_buffers &s, rendering::extent2d target,
                         std::int32_t origin_x, std::int32_t origin_y) const;
        void draw_range(const rendering::command_list &cl, const slot_buffers &s, const render_batch &batch,
                        layer_id within, rendering::extent2d target, std::int32_t origin_x, std::int32_t origin_y,
                        std::uint32_t composite_first_index) const;

        rendering::device device_{};
        rendering::pipeline pipeline_{};
        rendering::sampler sampler_{};
        rendering::texture white_{};
        std::vector<slot_buffers> slots_;
        std::vector<texture_entry> textures_;
        std::vector<vertex> composite_vertices_;
        std::vector<index> composite_indices_;
        renderer_desc desc_{};
    };

} // namespace catalyst::ui
