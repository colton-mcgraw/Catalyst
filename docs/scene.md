# Catalyst scene subsystem

Status: Tier 1 (world, transforms, components, extraction) implemented 2026-09-13. Tiers 2+ are
planned. This document is the plan and the record of what has landed, the role `docs/rendering.md`
plays for the renderer and `docs/ui.md` for the UI.

`catalyst::scene` is the **3D** half of the pair `catalyst::ui` completes. Where the UI module is a
retained tree of styled boxes laid out in two dimensions, the scene module is a retained set of
entities with a transform hierarchy, components, and a culling and sorting pass that turns them into
a list a renderer can draw. Both end the same way: a flat, backend-agnostic list, and no call into a
graphics API from the module itself.

## Shape of the subsystem

```
    app                       catalyst_scene                        catalyst_scene_renderer
  ┌────────┐              ┌───────────────────────────────┐        ┌───────────────────────┐
  │ create │  entities    │  world                        │        │  catalyst::rendering  │
  │ edit   │─────────────▶│   slot pool + hierarchy       │        │  pipelines / buffers  │
  │ destroy│  components  │   transform (local) ──┐       │        └───────────────────────┘
  └────────┘              │   component pools     │       │                    ▲
                          │                       ▼       │                    │
                          │   update_transforms ──▶ world │                    │
                          │       dirty subtrees only     │                    │
                          │                       │       │                    │
  camera entity ─────────▶│   make_view ──▶ render_view   │                    │
                          │                       │       │                    │
                          │   extract ──▶ render_list ────┼────────────────────┘
                          │     frustum cull, depth,      │    mesh_id / material_id
                          │     material sort             │    resolved by the bridge
                          └───────────────────────────────┘
```

A frame:

```cpp
using namespace catalyst::scene;

world w;
const entity player = w.create("player");
w.mutable_local(player).position = {0.0f, 0.0f, -5.0f};

renderable &r = w.add<renderable>(player);
r.mesh = mesh_id{mesh_table.id_of("ship")};
r.material = material_id{material_table.id_of("hull")};
r.local_bounds = aabb::from_center_extents({0, 0, 0}, {1.5f, 0.5f, 2.0f});

const entity cam = w.create("camera");
w.add<camera>(cam).fov_y = math::radians(60.0f);
w.set_local(cam, looking_at({0.0f, 4.0f, 10.0f}, {0.0f, 0.0f, 0.0f}));

render_list list;                            // reused every frame; steady state allocates nothing
while (running)
{
    simulate(w, dt);                         // ordinary component edits
    w.update_transforms();                   // dirty subtrees only

    const auto view = make_view(w, cam, aspect);
    extract(w, *view, {}, list);             // cull, depth, sort

    for (const draw_item &item : list.items)
        submit(item.mesh, item.material, item.world);
}
```

### The five choices that define it

**1. The world is `ui::tree` plus components.** Entities live in a slot pool, handles are index plus
generation, and an accessor given a stale handle degrades to a no-op rather than crashing: read
accessors return a shared default, write accessors return a scratch slot that is discarded. That is
exactly how `ui::tree` behaves and how `input::context` and the rendering device's handles behave,
so there is one story in the engine for "what happens to a handle you kept too long" rather than
three. It also means the two big retained structures in the project read the same way, which is
worth more than either being individually optimal.

**2. Components are a sparse set per type, with no registration.** `w.add<health>(e)` creates the
pool on first use; a component needs no base class, no traits and no virtuals, only to be movable.
Same bar `resource::registry<T>` sets. Dense arrays keep `each<T>` contiguous, which is the whole
point of storing components apart from entities; the sparse array makes lookup one indirection.
Removal swaps the last element into the hole, so pool order is not insertion order and is not
stable — anything that needs an order sorts the extracted list, not the pool.

This is a sparse-set ECS, not an archetype one. Archetypes win on multi-component iteration and lose
on structural change; at the scale a scene graph actually runs — thousands of entities, edited
freely by gameplay code — the trade goes the other way, and the sparse set is a quarter of the code.

**3. Transforms are stored decomposed, composed on demand, cached per entity.** A `transform` is
position, rotation and scale, because that is how it is edited: a camera controller writes
`rotation`, a spawner writes `position`, and neither wants to decompose a matrix first.
`update_transforms` walks the hierarchy top-down over an explicit stack and recomputes only the
subtrees whose local transform changed or whose ancestor's did. A clean subtree costs one flag test
per entity.

Because it is the only place world matrices change, `world_of` is a plain read and `extract` takes a
`const world &`. Extraction for several views can therefore run on several threads against one
world with no locking, which is the reason the pass was shaped this way rather than as a
recompute-as-you-go.

**4. The scene names meshes and materials but never opens them.** `mesh_id` and `material_id` are
`rendering::resource_handle` with scene-local tags, for the reason `catalyst::resource` re-exports
the same handles: the project has one handle vocabulary and a second would only need converting.
`<catalyst/rendering/types.hpp>` is constexpr-only, so this is an **include-path dependency, not a
link one** — `catalyst_scene` links `catalyst::math` and nothing else, and the whole module is
testable with no device and no GPU. What an id refers to is the application's business until the
renderer bridge lands.

**5. Extraction produces a value, not a traversal.** `extract` fills a `render_list` of `draw_item`s
that each carry a world matrix, world-space bounds, a depth and a sort key — copies, not pointers
into the world. A list extracted on one thread can therefore be consumed on another while the world
is being edited for the next frame, and two extractions of an unchanged world produce byte-identical
lists, which is what makes per-frame diffing possible later. The sort is stable for the same reason.

The sort key is 8 bits reserved for the pass, then the low 32 bits of the material id, then 24 bits
of depth. Ascending, that groups by material — which is what minimises pipeline and descriptor
changes — and orders front to back within a material, which is what lets early depth testing reject
the most fragments. `make_sort_key` is public so a custom pass can produce keys that sort
consistently with the built-in ones.

---

## Conventions this module inherits, and the one it sets

| | |
|---|---|
| Right-handed, +y up, an unrotated entity looks down **-z** | Matches `math::look_at`, so a camera with an identity rotation needs no correction matrix |
| Row-major matrices, depth in `[0, 1]` | What `catalyst::math` produces; the frustum extraction depends on both |
| `near_plane` / `far_plane`, never `near` / `far` | `<windows.h>` defines both as macros, and a public header that uses them breaks any TU that includes it afterwards |
| An **empty** `aabb` is the default, not a zero box at the origin | Makes `merge` and `expand` need no first-point case, and makes "this renderable has no bounds" mean *never cull* rather than *always cull* |

That last one is the module's own invention and the one most likely to surprise: a default-constructed
`aabb` has `min` at +infinity and `max` at -infinity. Setting a renderable's `local_bounds` to `aabb{}`
is how it opts out of culling.

---

## Tiers

### Tier 1 — World, transforms, components, extraction ✅ implemented

| Header | Contents |
| --- | --- |
| `scene/entity.hpp` | `entity` (index + generation), `null_entity`, `is_null`, `std::hash` |
| `scene/transform.hpp` | `transform` (position/rotation/scale), `world_right`/`up`/`forward`, `look_rotation`, `looking_at` |
| `scene/bounds.hpp` | `aabb`, `sphere`, `plane`, `frustum`, `transformed`, `bounding_sphere` |
| `scene/camera.hpp` | `projection_kind`, `camera`, `view_matrix` |
| `scene/light.hpp` | `light_kind`, `light` |
| `scene/renderable.hpp` | `mesh_id`, `material_id`, `renderable` |
| `scene/world.hpp` | `component` concept, `detail::pool<T>`, `class world` |
| `scene/render_list.hpp` | `render_view`, `make_view`, `draw_item`, `light_item`, `render_list`, `extract_params`, `make_sort_key`, `extract` |
| `scene/scene.hpp` | umbrella |

Implemented: entity slot pool with generation handles and names; parent/child hierarchy with cycle
rejection and a root list; per-type component pools with `add`/`get`/`has`/`remove`/`count`/`each`;
local transforms with dirty tracking and top-down world propagation; transform compose and decompose
(including the mirrored case); `look_rotation` with a degenerate-up fallback; AABB merge, expand and
conservative transform by Arvo's method; Gribb-Hartmann frustum extraction for `[0, 1]` depth with
p-vertex box tests; perspective and orthographic cameras with a per-camera aspect override; view
construction from a camera entity or from raw matrices; and extraction with frustum culling, layer
masks, depth along the view axis and a stable material-then-depth sort.

Tests: `catalyst.scene.{world,transform,bounds,extract}`.

Example: `examples/scene_graph` — a spinning carousel of parented entities, a camera, a directional
sun and a point light riding the hierarchy, run through the per-frame flow and logged. It links only
`catalyst::scene` and `catalyst::logging` and opens no window, which is the Tier 1 claim made
executable: the whole of a frame's scene work happens before anything touches a device.

Deliberately deferred inside Tier 1: world-preserving reparent (`set_parent` keeps the *local*
transform, which is what a spawner attaching a projectile to a muzzle wants), multi-component
iteration (`each<A, B>`), and any spatial index — a linear pass over a dense `renderable` pool is
faster than a BVH until the entity count is well past what this module has been asked to hold.

### Tier 2 — Renderer bridge

The counterpart of the UI module's Tier 5, and the same shape: a new target
`catalyst_scene_renderer` (`src/scene/renderer/`) behind `CATALYST_BUILD_SCENE_RENDERER`, linking
`catalyst::scene` and `catalyst::rendering`. It resolves `mesh_id` and `material_id` against tables
the application registers, uploads per-object constants, records a `render_list` into a
`command_list`, and drives a `frame_ring`. An `examples/scene_basics` app that opens a window and
draws a lit, culled scene is the acceptance test.

This is where the ids stop being opaque. It is a separate target precisely so that everything above
stays headless.

### Tier 3 — Visibility that scales

- A spatial index behind `extract`, chosen once there is a benchmark to choose it with: a BVH over
  the `renderable` pool, refit rather than rebuilt for entities whose bounds changed.
- Occlusion: a depth pyramid from the previous frame, tested per item before the material sort.
- Hierarchical culling — skip a subtree whose combined bounds are outside — which needs subtree
  bounds cached on the world and invalidated alongside the transform dirty flag.

### Tier 4 — Materials and passes

`material` as a real type rather than an id: a pipeline, a parameter block, and a pass mask that
says which of opaque, transparent, shadow and depth-prepass it takes part in. `extract` gains a pass
parameter and the sort key's reserved byte starts being used. Transparent items sort back to front,
which is the one case where the depth ordering must invert.

### Tier 5 — Content

- `scene/loader.hpp`: glTF into a world, built on `catalyst::resource`'s loader seam. The OBJ parser
  already in `resource` is the smaller version of the same thing.
- Instancing: one `draw_item` carrying a span of transforms, emitted when consecutive items share a
  mesh and material.
- Skinning and skeletal animation, once `catalyst::animation` exists to drive it.

### Tier 6 — Beyond

Scene serialisation, a scene-graph inspector built on `catalyst::ui`, LOD selection from the
extracted depth, and `catalyst::physics` integration, which wants exactly the transform hierarchy
this module already maintains.

---

## Deliberately deferred, with reasons

- **A thread pool.** Extraction is safe to run on several threads, but the module starts none. Same
  decision as the rendering and audio modules: the application already has a scheduler, and importing
  a second one is how two of them end up fighting.
- **`each<A, B>` over two component types.** Easy to add over the sparse sets, and nothing in Tier 1
  needs it. Adding it on the first real caller means it gets the signature that caller wants.
- **Archetypes.** See choice 2. If a profile ever shows multi-component iteration dominating, the
  storage can change behind `each<T>` without the public API moving.
- **An event stream for entity lifetime.** `catalyst::events` is right there and a `entity_destroyed`
  event would be three lines, but nothing consumes one yet, and an event nobody listens to is a
  contract that gets broken silently.

---

## Conventions

- Headers under `include/catalyst/scene/`, implementation under `src/scene/`, tests under
  `tests/scene/` with one executable per test file and a `catalyst.scene.<name>` CTest name.
- `#include <catalyst/scene/scene.hpp>` pulls in the whole module.
- Handles are index + generation, so a stale `entity` is detected rather than aliasing a recycled slot.
- A world is owned by one thread. Nothing locks; `extract` is `const` and safe to run concurrently.
