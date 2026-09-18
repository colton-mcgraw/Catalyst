# Catalyst UI subsystem

Status: Tier 1 implemented 2026-09-02. Tiers 2 (painting), 3 (input and interaction) and 4 (the
text seam) implemented 2026-09-13. Tier 5 (the renderer bridge) implemented 2026-09-17: draws,
textures and group-opacity layers.
Tiers 6+ are planned.

## Shape of the subsystem

`catalyst::ui` is a **retained, declarative** UI module. Applications build a persistent
tree of nodes, give each node a `style` expressed in CSS-like units, and ask the module to
lay it out. The module then emits a backend-agnostic draw list; it never talks to a GPU
itself.

```
    app                catalyst_ui                       catalyst_ui_renderer
  ┌───────┐   style   ┌──────────────────────────────┐   ┌──────────────────────┐
  │ tree  │──────────▶│ layout → paint → render_batch│──▶│ catalyst::rendering  │
  │ edits │           │        ▲            ▲        │   │ pipelines / buffers  │
  └───────┘           │  measure_fn    font/glyph    │   └──────────────────────┘
      ▲               │   (text seam)    provider    │
      │  ui events    └──────────────────────────────┘
      └───────────────────── hit-test / focus routing
```

### Why these four choices

- **Retained + declarative.** `measurement.hpp` already models the CSS unit system
  (`px`/`dp`/`em`/`rem`/`%`/`vw`/`vh`/`in`/`cm`/`mm`, `auto`, and `calc`-style linear
  combinations). An immediate-mode design would leave most of that unused. A retained tree
  also gives us stable identity for focus, hit-testing, animation and accessibility.
- **Draw list, not direct GPU calls.** `catalyst_ui` depends only on `catalyst::math`
  (and `catalyst::core` for events, from Tier 3). Layout and paint are testable headless, with
  no device. A separate optional `catalyst_ui_renderer` target links `catalyst::rendering`
  and submits the batch. This keeps the module split the rest of the repo already follows.
- **Text seam now, text engine later.** Layout asks a node for its intrinsic size through a
  `measure_fn` callback. Text is just a node with a measure callback, so the real font stack
  can land in Tier 4 without touching the layout engine.
- **Box-sizing defaults to `border_box`.** `width`/`height` include padding and border. This
  differs from CSS's `content_box` default; it is what UI code almost always wants, and
  `box_sizing::content_box` is available per node.

## Tiers

### Tier 1 — Layout core ✅ implemented

| Header | Contents |
| --- | --- |
| `ui/measurement.hpp` | (pre-existing) `length`, `calc_measure`, `resolve_context`, `resolve_or`, unit literals |
| `ui/geometry.hpp` | `point`, `extent`, `rect` aliases over `catalyst::math`; `edges<T>`, `corners<T>`; `inflate`/`deflate` |
| `ui/color.hpp` | linear float RGBA `color`, `rgba8` packing, `lerp`, named `colors` |
| `ui/style.hpp` | `display_mode`, `position_mode`, `box_sizing`, `flex_direction`, `justify`, `align`, `overflow_mode`, and the `style` struct |
| `ui/node.hpp` | `node` handle (index + generation), `layout_result`, `measure_fn` / `paint_fn` seams, `class tree` |
| `ui/layout.hpp` | `layout_params`, `layout()` — single-line flexbox + absolute positioning |

Implemented layout features: flex row/column (+ reverse), `flex_grow`/`flex_shrink`/
`flex_basis`, `justify_content` (all six), `align_items`/`align_self` (including `stretch`),
`gap`, margin/border/padding (percentages resolve against the containing block width, per
CSS), `min_*`/`max_*` clamping, both box-sizing modes, `display_mode::none`,
`position_mode::absolute` against the parent's padding box, and the
`measure_fn` intrinsic-size seam.

Deliberately deferred: flex wrap, baseline alignment, grid, and incremental (dirty-driven)
relayout. Dirty flags are already tracked, they are just not yet used to skip subtrees.

### Tier 2 — Painting ✅ implemented

| Header | Contents |
| --- | --- |
| `ui/batch.hpp` | `vertex` (position, uv, packed color), `index`, `texture_key`, `layer_id`, `draw_command`, `layer`, `render_batch`, `batch_builder`, `corner_segments` |
| `ui/paint.hpp` | `opacity_mode`, `paint_params`, `paint_context`, `paint()` |

`batch_builder` owns a clip stack, culls whole shapes that miss the clip, and merges geometry into
the previous `draw_command` whenever the clip and texture have not changed, so a screen of solid
boxes is one draw. It tessellates: `add_rect` (with or without UVs), `add_rounded_rect`,
`add_border`, `add_line`, and raw `add_triangles` for content painters that build their own geometry.

Three decisions worth keeping:

- **Corners are tessellated on the CPU, not in a shader.** A shader would tie the batch format to
  one renderer. The cost is small: `corner_segments` picks enough segments to hold the chord error
  under about half a pixel, clamped to 32, and a zero-radius corner collapses to a single point, so
  the overwhelmingly common square border costs eight vertices rather than eight duplicates.
- **Radii are clamped by one factor for all four corners**, as CSS specifies. Scaling corners
  independently leaves a visible seam where two differently scaled arcs meet.
- **`texture_key` is a `std::uint64_t`, not a `rendering::texture`.** It keeps `catalyst_ui` from
  linking the rendering module, and lets a text provider hand out atlas ids without agreeing on a
  handle type with a renderer that does not exist yet.

`paint()` walks a laid-out tree emitting background, then border, then the node's own `paint_fn`,
then its children, pushing a clip to the padding box for anything whose `overflow` is not `visible`.

Opacity has two modes, chosen by `paint_params::compositing`. The default, `opacity_mode::group`, is
CSS's: a translucent node opens a **layer** (`batch_builder::begin_layer` over the node's
`subtree_bounds`, cut to the clip), its subtree paints at full alpha inside, and the renderer draws
the layer offscreen and blends the finished image in at the node's opacity, so overlapping children
do not show through each other. `opacity_mode::multiply` is the cheap alternative from before
2026-09-17: the opacity multiplies into every descendant's alpha, no layer, no offscreen pass. A
`render_batch` therefore carries `layers` beside its commands, each command names the innermost
layer it was recorded in, and the builder never merges a command across a layer boundary. A consumer
that cannot draw layers should ask for `multiply` rather than ignore them.

Still open in this tier: damage rectangles, so a static frame costs no vertex work. The dirty flags
that would drive them are already tracked.

### Tier 3 — Input and interaction ✅ implemented

| Header | Contents |
| --- | --- |
| `ui/hit_test.hpp` | `hit_test()`, point to the deepest and topmost node |
| `ui/events.hpp` | the `tags` block at `0x0004'0000`, `pointer_button`, `modifiers`, and twelve event types |
| `ui/interaction.hpp` | `class interaction`: hover, press, click, capture, focus |

`hit_test` walks in reverse paint order, so the last-painted sibling wins, and it honours `overflow`
(a clipping node does not let a point reach children outside its padding box) and the new
`style::pointer_events`, which lets a full-screen overlay pass clicks through while its own buttons
still work.

`interaction` is fed raw input by the application, a position or a button or a key, and publishes
`pointer_enter_event`, `click_event`, `focus_gained_event` and the rest **on a
`catalyst::events::bus`**, like every other event in the engine, rather than calling back into widget
code. A widget and an application therefore listen the same way, and middleware can swallow an event
before the widget sees it. Every stored handle is re-validated on entry, so destroying the hovered or
focused node between two calls drops the handle silently instead of delivering a leave to a node that
no longer exists.

`pointer_button` and `modifiers` are the module's own vocabulary rather than `input::mouse_button`,
so `catalyst_ui` does not link `catalyst::input` and a touch or pen source can feed the same events.

Still open in this tier: tab order. The bridge that turns `catalyst::input` events into these calls
lives in `examples/ui_sample/main.cpp` (2026-09-17): a dozen lines mapping `mouse_button` to
`pointer_button` and `key_modifiers` to `modifiers`, registered as bus listeners. It stays in the
example rather than the module so `catalyst_ui` keeps not linking `catalyst::input`.

### Tier 4 — Text ✅ seam and layout implemented (a real font engine is not)

| Header | Contents |
| --- | --- |
| `ui/text.hpp` | `font_id`, `font_metrics`, `glyph`, `text_provider`, `null_text_provider`, `text_style`, `text_layout`, `layout_text`, `measure_text`, `text_content` |

The seam is three questions, how tall is a line, what does this code point look like, and do these
two glyphs kern, and nothing about files, rasterisation or atlases, which are the provider's own
business. Shaping is deliberately absent: a shaping provider would expose a run-level call, and
adding one later does not change these three.

`layout_text` does greedy line breaking against that seam. A word that does not fit moves whole to
the next line, a word longer than a line breaks where it overflows, newlines always break, and
malformed UTF-8 decodes to U+FFFD. `text_content` wires both halves of the content seam onto a node
at once: `measure_fn` so layout sizes the node from the text, `paint_fn` so the paint pass draws it.

`null_text_provider` has no font, and every glyph is a monospaced box at the usual proportions of a
Latin face. It is what the tests run against, and what an application gets before a real provider is
wired in: text takes up plausible space and paints as tofu rather than vanishing, which is easier to
debug than nothing.

Adding this tier sent one change back into `catalyst::text`. For all the UTF-8 **encoding** that
module did, nothing in the tree **decoded**, and walking a label by code point needs it.
`utf8::decode` now sits beside `utf8::encode`, rejecting overlong forms and resynchronising one byte
at a time on bad input.

`examples/ui_sample/font_provider.hpp` (2026-09-17) is a working provider outside the module: a
`text_provider` over stb_truetype, which rides along with the resource module's stb fetch. It maps
an em to `size_px` pixels, rasterises each (size, code point) the first time it is asked for into a
CPU-side `r8` atlas with a shelf packer, and reports two things the application acts on: `take_dirty`
when pixels changed, so the atlas is uploaded again, and `take_resized` when it grew, since every
`uv` handed out before then points at the old size and the batch must be painted again. It is an
example, not the module's answer; it shows the seam is enough to set real text.

Still open in this tier: a provider in the module, `stb_truetype` vendored in-tree or platform
backends (DirectWrite, CoreText, FreeType) behind `CATALYST_UI_TEXT_BACKEND`, mirroring the audio
and input pattern, with the atlas management the sample does by hand.

### Tier 5 — Renderer bridge ✅ implemented (first cut 2026-09-17, textures and layers the same day)

| Header | Contents |
| --- | --- |
| `ui/renderer.hpp` | `renderer_desc`, `class renderer`: `create`, `register_texture`, `prepare`, `render`, `destroy`, per-slot capacities |

The target is `catalyst_ui_renderer` (`src/ui/renderer/`), behind `CATALYST_BUILD_UI_RENDERER`, a
dependent option that is ON whenever both `catalyst::ui` and `catalyst::rendering` are. It is the one
place the two modules meet: `catalyst_ui` still does not link the rendering module, and the header
guard is `CATALYST_HAS_UI_RENDERER`. Tests are `catalyst.ui.renderer`, which need a device like the
rendering module's tests and take the same `CATALYST_RENDERING_VALIDATION` switch.

`renderer::create` builds one graphics pipeline for the attachment format it is told, from SPIR-V
checked in as `src/ui/renderer/shaders.hpp` (regenerate with `scripts/embed_spirv.py` after editing
`shaders/ui.vert` or `ui.frag`), a sampler, a white 1x1 fallback texture, and one host-visible vertex
and index buffer per frame in flight. `render(cl, batch, viewport, slot)` writes the batch into that
slot's buffers, growing them and never shrinking them, then records the pipeline, the pixel-to-clip
push constants, the buffers and sampler, and per `draw_command` a texture binding when it changes, a
scissor clamped to the viewport and one `draw_indexed`. Commands whose clip misses the viewport are
skipped. Slots exist so that frame N+1 never overwrites buffers the GPU is still reading for frame N:
the caller passes `frame::slot()` from the `frame_ring` that paces its loop.

**Textures.** `register_texture(key, texture)` tells the renderer which `rendering::texture` a
batch's `texture_key` names; the UI module still never sees a GPU handle, and a font provider can
stay CPU-side, rasterising into an atlas bitmap the application uploads and registers. What the
shader does with a texture follows its format: `r8_unorm` is a coverage mask whose red channel
scales alpha (a glyph atlas), anything else is a straight-alpha colour image multiplied by the vertex
colour. An unregistered key draws solid, so a batch never fails for want of a texture, and the
registry does not own what it maps.

**Layers.** A batch's `layer`s need an offscreen image each, drawn in a render pass of their own,
which cannot happen inside the caller's pass. So the frame has two calls: `prepare(cl, batch,
viewport, slot)` with the list recording but outside any pass uploads the batch, places every layer
(bounds cut to the viewport and snapped to whole pixels), and draws each one, innermost first, into
that slot's image for it; then `render` inside the pass draws the root commands and, where a layer
starts in the command order, one textured quad blended in at its opacity. Layer images live per slot
like the buffers, grow to the largest each index has held, and never shrink. A batch without layers
needs no `prepare`; one with layers is refused by `render` without it.

Four things worth knowing:

- **Colours go through linear.** `ui::color` is linear, the vertex attribute is `rgba8_unorm`, and
  an sRGB swapchain encodes on write. A palette written as sRGB bytes must be decoded first, as
  `examples/ui_sample` does; feeding the bytes straight in draws them too light.
- **The push constants are `{scale, translate, mode, opacity}` with a negative y scale**, because
  the Vulkan backend flips its viewport to make clip-space y point up. Pixel row 0 lands at the top.
  `mode` and `opacity` are pushed again per draw only when they change.
- **Blending is premultiplied.** The fragment stage multiplies colour by alpha and the pipeline
  blends `(one, one_minus_src_alpha)`, which draws the same as straight alpha for ordinary geometry
  and is what lets a layer image, drawn onto transparent, composite correctly when sampled back.
- **A layer image has the attachment's format.** The one pipeline serves both passes, so the
  format must be samplable, which every 8-bit colour format is. Group opacity on an sRGB target
  therefore stores premultiplied colour sRGB-encoded, which is exact enough at 8 bits.

`examples/ui_sample` is the window that uses it: layout to the swapchain, paint on change, `prepare`
then `render` every frame, and the input bridge from Tier 3. Its "toast" panel is a translucent group;
`G` toggles between `group` and `multiply` so the difference is visible. Its text goes through the
texture path: the sample's stb_truetype provider fills an `r8` atlas, and after each paint the loop
uploads a changed atlas as a new sampled texture, registers it under the glyphs' key, and retires the
previous texture once the frame ring has moved `frames_in_flight` frames past the last draw that
could have sampled it. The renderer never owns the atlas.

Still open in this tier: nothing. A font provider inside the module is Tier 4's open item (the
sample carries its own), and pixel readback of a texture, which would let the renderer tests check what they drew
rather than only that it recorded cleanly, is the rendering module's.

### Tier 6 — Widgets

Button, label, image, checkbox, radio, slider, text field, scroll view, list/virtualized
list, splitter. A `theme` that supplies default styles, plus style inheritance for font
properties.

### Tier 7 — Beyond

`catalyst::animation` integration for style transitions, accessibility metadata hooks,
flex wrap and grid layout, layout benchmarks in `bench/ui`.

## Conventions

- Headers under `include/catalyst/ui/`, implementation under `src/ui/`, tests under
  `tests/ui/` with one executable per test file and a `catalyst.ui.<name>` CTest name.
- `#include <catalyst/ui/ui.hpp>` pulls in the whole module.
- `catalyst_ui` links `catalyst::math` and `catalyst::events` publicly and `catalyst::text`
  privately. It does **not** link `catalyst::rendering`, and must not: that is Tier 5's target.
- Handles are index + generation, so a stale `node` is detected rather than aliasing a
  recycled slot.
