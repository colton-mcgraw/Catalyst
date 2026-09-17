# Catalyst UI subsystem

Status: Tier 1 implemented 2026-09-02. Tiers 2 (painting), 3 (input and interaction) and 4 (the
text seam) implemented 2026-09-13. Tiers 5+ are planned.

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
| `ui/batch.hpp` | `vertex` (position, uv, packed color), `index`, `texture_key`, `draw_command`, `render_batch`, `batch_builder`, `corner_segments` |
| `ui/paint.hpp` | `paint_params`, `paint_context`, `paint()` |

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
Opacity multiplies down the tree per node. That is *not* group opacity: overlapping children show
through each other, and true group compositing needs an offscreen pass, deferred with the renderer
bridge.

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

Still open in this tier: tab order, and the bridge that turns `catalyst::input` events into these
calls. A dozen lines, but it belongs with the example that needs it.

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

Still open in this tier: a real provider, `stb_truetype` vendored in-tree or platform backends
(DirectWrite, CoreText, FreeType) behind `CATALYST_UI_TEXT_BACKEND`, mirroring the audio and input
pattern, and the glyph atlas it would fill.

### Tier 5 — Renderer bridge

- New target `catalyst_ui_renderer` (`src/ui/renderer/`), guarded by
  `CATALYST_BUILD_UI_RENDERER`, linking `catalyst::ui` and `catalyst::rendering`.
- UI pipeline + shaders, dynamic vertex/index buffers, atlas texture upload, scissor from
  each `draw_command`'s clip rect.
- An `examples/ui_basics` app that lays out and draws a real window, and the
  `catalyst::input` to `interaction` bridge it needs.
- Group opacity, which needs the offscreen pass Tier 2 deferred.

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
