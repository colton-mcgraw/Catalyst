# Changelog

All notable changes to Catalyst are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the version numbers follow the
policy in the README under *Versioning and compatibility*: while Catalyst is pre-1.0, a minor
release may break the API and a patch release may not.

## [Unreleased]

### Changed

- `ui_sample`: every length in the tree, the default text size and the window size are now in
  `dp`, so the sample follows the monitor's DPI scale instead of drawing 16-pixel text on a
  high-DPI panel. "+", "-" and "0" zoom the whole tree, and "P" shows a profiler panel with the
  CPU time of each phase of the frame loop (events, layout, paint, atlas upload, frame-ring wait,
  acquire, record, submit, present) over the last half second, plus the batch and atlas sizes. A
  one-line summary goes to the log every five seconds.
- Win32 platform backend: a window being dragged or resized is now paced by a helper thread that
  posts to it once per compositor frame (DwmFlush), so a stationary drag renders at the display's
  rate instead of the ~64 Hz the size/move timer could manage. The timer remains as a fallback
  when the thread cannot be started. The pacing gate uses the refresh period of the monitor the
  window is on rather than the primary display's.
- Win32 platform backend: `WM_SIZE` requests a frame only while a modal size/move loop is running.
  A `set_client_size`, snap or maximize outside one no longer renders an unpaced extra frame
  before the application's own loop does.
- Win32 platform backend: a finite `wait_events` timeout is honoured to within a fraction of a
  millisecond, on a high-resolution waitable timer where Windows 10 1803 or later provides one.
  It previously rounded up to the 15.6 ms scheduler tick.
- Win32 platform backend: monitor queries are answered from a cache that is dropped on
  `WM_DISPLAYCHANGE`, `WM_DPICHANGED` and `WM_SETTINGCHANGE`, and `primary_monitor` asks the OS
  directly. Each query used to enumerate every monitor and create a display DC per monitor.
- Win32 platform backend: modifier state is sampled with one `GetKeyboardState` call per input
  message instead of seven `GetKeyState` calls, each of which is a kernel transition.
  `TranslateMessage` is skipped for non-key messages, `is_valid` no longer round-trips through
  `IsWindow`, and a captured window being moved or resized only re-clips the cursor instead of
  re-running the full cursor-mode routine.

### Fixed

- `wait_events` documentation in the platform module described a zero timeout as waiting forever;
  the implementation and the public header use `0xFFFFFFFF` for that and treat zero as a poll.

## [0.1.0] - 2026-09-18

Initial development release: rendering, resource, audio, input, logging, platform, math, ui,
scene, events and text modules, with Win32 and Vulkan backends and null backends for every module.
The entries below are the build-system and documentation work that went into cutting it.

### Added

- `cmake/CatalystModules.cmake`: one declaration per module, from which the build switches,
  build order, monolithic link list, `<catalyst/config.hpp>`, install export and test tree are all
  derived. Adding a module is one `catalyst_module()` call plus its directories.
- `cmake/CatalystSparseCheckout.cmake`: works out the `git sparse-checkout` paths for a set of
  modules and their dependencies, so a consumer can fetch only the sources they build. The build
  tolerates a checkout that left modules, tests, examples or benchmarks out.
- `CATALYST_DEVELOPER_MODE`: ON when Catalyst is the top-level project, OFF when it is pulled in
  with `add_subdirectory()` or `FetchContent`. The examples, tests and benchmarks default to it.
- `tests/consumer/`: a committed project that consumes an installed Catalyst through
  `find_package()`, replacing the shell heredoc CI used to write. It also checks that a component
  the install lacks is reported absent, and CI checks that requiring one fails.
- Test suites for `catalyst::core` (version string, `move_only_function`) and `catalyst::platform`
  (window and monitor API against the null backend).
- This changelog, and a written compatibility policy in the README.
- `.gitattributes` marking contributor-only files `export-ignore`, so release tarballs do not
  carry them.

- Event bus tests for callable lifetime after removal, registries with hundreds of event types and
  table growth under concurrent dispatch; benchmark cases for a four-layer middleware chain and for
  a type nothing is registered for.

### Changed

- CMake 3.23 is the minimum, everywhere: the project, the presets (whose schema already needed it),
  the consumer fixture and the docs. The old floor of 3.16 was never true, since `FetchContent`'s
  `SOURCE_SUBDIR` needs 3.18.
- The examples, tests and benchmarks default to OFF when Catalyst is a dependency of another
  project. In a checkout configured directly nothing changes.
- Placeholder modules (`animation`, `net`, `physics`, `utils`) are not installed while switched
  off, headers included. A default install carries no empty public namespace, and the README says
  they are outside the compatibility policy.
- `catalyst_require_modules()` takes only the module's name; its dependencies come from the
  manifest instead of being repeated in each module's `CMakeLists.txt`.
- `catalyst::rendering` declares its dependency on `catalyst::logging`. Switching logging off with
  rendering on now fails with a sentence naming the switch instead of a missing-target error.
- CI: the module-subset job also builds without logging, builds a sparse checkout of two modules,
  and drives the committed consumer fixture. `CONTRIBUTING.md` describes all of it.
- `events::bus::dispatch` takes no lock and does no reference counting. The registry is a flat
  table of per-type entries, each publishing immutable lists, read under sharded per-thread
  counters with deferred reclamation in the style of RCU; the event-type hash is computed once per
  type rather than on every dispatch (on the MSVC STL `type_index::hash_code()` re-hashes the
  mangled name each call); and callables live inline in their slot behind one thunk instead of a
  `move_only_function`. Release benchmark: an empty dispatch from about 90 ns to 15 ns, one
  listener from 110 ns to 22 ns, eight dispatching threads from 190 ns/op to 5 ns/op. One
  observable nuance: a registration removed while any dispatch on that bus is in flight is freed by
  the next add or remove, or by the bus's destructor, rather than the moment that dispatch returns.
- `events::token` is 8 bytes larger: it carries the hashed event key so removal does not rehash.

### Fixed

- README: the build-all script is `scripts/build-all-available-presets.ps1` and takes `-RunTests`
  only; the module switches do not all default to ON; the CMake floor matches the project.
- Linking `catalyst::rendering` with the Vulkan backend failed on Linux with an undefined
  reference to `error::message`: the backend static library calls into the frontend but only the
  frontend linked the backend, so GNU ld saw the reference after it had already left the frontend
  behind. Each backend now links `catalyst_rendering` too; CMake handles the cycle between static
  libraries by repeating them on the link line.
- The `sandbox` example is gated on `platform` and `ui`, which it uses, and links `platform` when
  the monolithic library is off. Before, `-DCATALYST_BUILD_ALL=OFF` failed at link time.
- `tests/resource/test_json.cpp` no longer puts raw string literals containing an escaped quote
  inside a macro argument, which MSVC's preprocessor mis-lexes.
- The tree is clang-format 18 clean, which is what CI checks.

### Known issues

- `catalyst.rendering.transfer` fails in a Release build against the Vulkan backend: a
  `co_await` on a readback never resumes, so the test's bounded pump loop gives up
  (`tests/rendering/test_transfer.cpp`, "co_await readback"). Reproduced on 2026-09-18 with
  clang-cl and an Intel Arc GPU, three runs out of three. Debug builds and the `null` backend pass.
  Until it is fixed, treat readback completion on Vulkan in optimised builds as unreliable.

[0.1.0]: https://github.com/colton-mcgraw/Catalyst/releases/tag/v0.1.0
