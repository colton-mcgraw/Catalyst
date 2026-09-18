# Catalyst

Catalyst is a C++23 multimedia library: audio, rendering, input, windowing, asset loading, UI
layout, logging and maths, as modules you can take individually or together.

It is version 0.1.0 and under active development. The table below says plainly which parts are
real, because a list of ambitions is no use to someone deciding whether to build against it.

## Status

| Module | State | What it is |
| --- | --- | --- |
| `rendering` | Working | Device, queues, timeline sync, buffers, textures, pipelines, swapchain, transfer. Vulkan backend is real; d3d12 and metal are stubs. |
| `resource` | Working | VFS, loaders, URIs, JSON, CSV, OBJ, images (PNG/JPEG via stb, KTX2 and DDS natively), mip generation. |
| `audio` | Working | Device enumeration, streams, mixing, offline rendering. WASAPI and ASIO backends. |
| `input` | Working | Keyboard, text, mouse with raw motion and capture, gamepads, action maps, calibration. |
| `logging` | Working | Levels, filters, middleware, routing, and console, terminal, file, ring-buffer, callback, async and queued sinks. |
| `platform` | Working | Windows, monitors, the event loop. Win32 backend. |
| `math` | Working | Vectors, matrices, quaternions, transforms, fractions, geometry. Header-only. |
| `ui` | Working | Retained tree of styled nodes, CSS-like units, flexbox layout, painting into a backend-agnostic draw list, hit testing, pointer and keyboard interaction, text layout. |
| `scene` | Working | Entities with a transform hierarchy and components, cameras, lights, bounding volumes, frustum culling and sorted extraction. |
| `events` | Working | The bus every other module publishes on. Header-only. |
| `text` | Working | UTF-8 encoding and decoding, and the scanner the parsers are built on. Header-only. |
| `core` | Minimal | Shared vocabulary. Currently just what more than one module needs. |
| `animation` | **Planned** | Not implemented. A placeholder: defaults to `OFF`, and is not installed while off. |
| `net` | **Planned** | Not implemented. A placeholder: defaults to `OFF`, and is not installed while off. |
| `physics` | **Planned** | Not implemented. A placeholder: defaults to `OFF`, and is not installed while off. |
| `utils` | **Planned** | Not implemented. A placeholder: defaults to `OFF`, and is not installed while off. |

Not started, and not currently planned for 0.1: scripting, a plugin system, and profiling tools.

The list above is also a file: [cmake/CatalystModules.cmake](cmake/CatalystModules.cmake) declares
every module once, and the build switches, build order, package export and test tree are all
derived from it.

## Platform support

| Platform | Window / input / audio | Rendering |
| --- | --- | --- |
| Windows | Win32, XInput, WASAPI + ASIO | Vulkan (real), d3d12 (stub) |
| Linux | **None yet** — the null backends build and test, but open no window and make no sound | Vulkan (real) |
| macOS | **None yet** | metal (stub) — and the library does not currently build with libc++; see Requirements |
| iOS, Android | Not started | Not started |

The renderer is platform-independent and the Vulkan backend works anywhere Vulkan does. What Linux
and macOS lack is the layer underneath: there is no X11, Wayland, Cocoa, ALSA, PulseAudio or
CoreAudio backend yet. On those platforms you get a real Vulkan renderer with no window to present
to, which is useful for offline and headless work and not much else.

Every module also has a `null` backend that builds and runs everywhere, which is what the test
suite uses.

## Requirements

Catalyst is written against C++23 and needs two features that are only just becoming widely
available: deducing this and `std::expected`.

| Toolchain | Status |
| --- | --- |
| GCC 14 or newer | Supported |
| Clang 19 or newer, with libstdc++ 14+ | Supported |
| MSVC 19.40 or newer (Visual Studio 2022 17.10+) | Supported |
| GCC 13 | No deducing this. |
| Clang 18 | libstdc++ gates `<expected>` on `__cpp_concepts >= 202002L`, which Clang only reports from 19. |
| libc++ / Apple Clang / macOS | **Not yet.** libc++ leaves the floating-point `std::from_chars` overloads deleted, and the JSON and CSV parsers need them. |

CMake checks both features at configure time and stops with a readable message rather than letting
the build fail hundreds of template errors later. On Ubuntu 24.04 the default `g++` is 13 and will
not work — install `g++-14` and point CMake at it:

```bash
sudo apt install g++-14
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-14
```

CMake 3.23 or newer is required; the presets file needs the same. Everything else is optional: see
[CMake Options](#cmake-options) for the module switches and
[Backend selection](#cmake-options) for what each backend needs.

## Getting Started

To get started with Catalyst, follow these steps:

1. Clone the repository:

   ```bash
   git clone https://github.com/colton-mcgraw/Catalyst.git
   ```

2. Navigate to the project directory:

   ```bash
    cd Catalyst
    ```

3. Build the project using CMake:

    ```bash
    cmake -S . -B build
    cmake --build build --config Release
    ```

4. Run an example:

    ```bash
    # Linux / macOS
    ./build/examples/audio_playback/catalyst_audio_playback

    # Windows
    .\build\examples\audio_playback\Release\catalyst_audio_playback.exe
    ```

A checkout configured like this is in *developer mode*: the examples, tests and benchmarks are
built too. When Catalyst is a dependency of another project they are not; see the next section.

## Using Catalyst from your project

There are four ways in, from least to most selective about what gets fetched. All four end the
same way: `target_link_libraries(app PRIVATE catalyst::audio catalyst::rendering ...)`, one
target per module, or `catalyst::catalyst` for everything that was built.

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(catalyst
  GIT_REPOSITORY https://github.com/colton-mcgraw/Catalyst.git
  GIT_TAG        v0.1.0      # a tag or a commit, never a branch
  GIT_SHALLOW    TRUE        # one commit, not the history
)
set(CATALYST_BUILD_UI OFF)   # any CATALYST_BUILD_* switch, before MakeAvailable
FetchContent_MakeAvailable(catalyst)

target_link_libraries(app PRIVATE catalyst::audio catalyst::logging)
```

As a dependency, `CATALYST_DEVELOPER_MODE` is off, so the examples, tests and benchmarks are not
built and nothing of theirs is fetched into your build graph. Only the modules you leave on are
compiled. The repository is small (under ten megabytes with history, less shallow), so for most
projects this is the right amount of selectivity.

### add_subdirectory or a submodule

The same, with the checkout under your control:

```bash
git submodule add https://github.com/colton-mcgraw/Catalyst.git external/catalyst
```

```cmake
set(CATALYST_BUILD_UI OFF)
add_subdirectory(external/catalyst)
```

### An installed package

Catalyst installs, and exports a CMake package, so it does not have to be in your tree at all:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/where/you/want/it
cmake --build build
cmake --install build
```

Then, from another project — every module is a component:

```cmake
find_package(Catalyst 0.1 REQUIRED COMPONENTS audio rendering math)

target_link_libraries(my_app PRIVATE catalyst::audio catalyst::rendering catalyst::math)
```

Asking for a module the install was not built with is an error naming what is missing and what the
install does contain, rather than a link failure later. `find_package` also sets `CATALYST_MODULES`
and `CATALYST_RENDERING_BACKEND_NAME` so a build can branch on either. The project in
[tests/consumer](tests/consumer/CMakeLists.txt) is a complete example, and CI builds it against a
fresh install.

### Fetching only what you need

If even a shallow clone is more than you want — a submodule in a repository that vendors several
libraries, say — take a sparse checkout of the modules you build. The layout is one directory per
module under `src/`, and the build tolerates a checkout that left any of them out: a module whose
sources are absent simply defaults to `OFF`.

```bash
git clone --filter=blob:none --sparse https://github.com/colton-mcgraw/Catalyst.git
cd Catalyst
git sparse-checkout add cmake
cmake -DMODULES="audio;logging" -DAPPLY=ON -P cmake/CatalystSparseCheckout.cmake
cmake -S . -B build
```

The helper reads the module manifest, adds each module's dependencies (here `events` and `core`),
and applies the result:

```text
-- Modules (with dependencies): core events audio logging
-- Sparse-checkout paths:       cmake include src/win32 src/core src/events src/audio src/logging
```

Run it without `-DAPPLY=ON` to see the paths and apply them yourself, or add
`-DEXTRA="tests;examples"` to take the test suites for those modules and the examples along. The
public headers under `include/` are always taken whole: they include across modules, and the tree
is small. `src/win32` is always taken too; it is five files, and the Win32 backends link it.

Asking for a module that is not in the checkout is an error that names the directory to add:

```text
CATALYST_BUILD_RENDERING=ON, but src/rendering/ is not in this checkout.
If this is a sparse checkout, add the module's sources:
  git sparse-checkout add src/rendering
```

The four header-only modules — `core`, `events`, `text` and `math` — need nothing under `src/`
beyond the target definition in their `src/<module>/CMakeLists.txt`, so a sparse checkout of any of
them is `include/` plus a few files.

### Multi-compiler builds (Windows)

This repo includes a root [CMakePresets.json](CMakePresets.json) so you can quickly validate builds across multiple compilers.

**Note** : MSVC builds are discouraged for developers looking for the most performance. The MSVC linker does not produce as optimized code as GCC or Clang in many cases.

- List presets:

  ```bash
  cmake --list-presets
  ```

- Build MSVC (Visual Studio):

  ```bash
  cmake --preset msvc-x64
  cmake --build --preset msvc-x64-debug
  cmake --build --preset msvc-x64-release
  ```

  Note about the MSVC generator:

  - The `msvc-x64` preset uses a specific CMake *Visual Studio* generator (e.g. `Visual Studio 18 2026`).
    If your installed Visual Studio version does not match the generator in the preset, CMake may report that it
    “could not find any instance of Visual Studio”.
  - If you change the generator, you must use a fresh build directory (or run `cmake --fresh --preset msvc-x64`) to
    avoid “generator does not match the generator used previously” errors.
  - To see which generator names are available on your machine:

    ```powershell
    cmake --help
    ```

- Build Clang-CL (Ninja):

  ```bash
  cmake --preset clangcl-x64-debug
  cmake --build --preset clangcl-x64-debug

  cmake --preset clangcl-x64-release
  cmake --build --preset clangcl-x64-release
  ```

- Build every preset available on this machine: see [Build-all scripts](#build-all-scripts).

### Test presets

Test-enabled configure/build presets are provided with the `-tests` suffix. They switch the
examples off and the tests on.

- Build + run tests (Windows):

  ```powershell
  ./scripts/build-all-available-presets.ps1 -RunTests
  ```

- Or run a specific test preset directly:

  ```bash
  # after building one of the *-tests build presets
  ctest --preset clangcl-x64-debug-tests
  ```

The `platform` suites run only against the null platform backend, because against Win32 they would
open real windows on your desktop; add `-DCATALYST_PLATFORM_BACKEND=null` to a configure to run
them on Windows.

### Cross-platform presets (Linux/macOS)

Presets are included for Linux (GCC/Clang) and macOS (Clang). These use the Ninja generator.
On each host OS, only the relevant presets are shown (Windows won’t list Linux/macOS presets, etc.).

Examples:

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug

cmake --preset macos-clang-release-tests
cmake --build --preset macos-clang-release-tests
ctest --preset macos-clang-release-tests
```

### Build-all scripts

Scripts are provided to auto-run all *available* presets on the current host:

- Windows: [scripts/build-all-available-presets.ps1](scripts/build-all-available-presets.ps1)
  - Build: `./scripts/build-all-available-presets.ps1`
  - Build + test: `./scripts/build-all-available-presets.ps1 -RunTests`
- Linux: [scripts/build-all-available-presets-linux.sh](scripts/build-all-available-presets-linux.sh)
  - Build: `./scripts/build-all-available-presets-linux.sh`
  - Build + test: `./scripts/build-all-available-presets-linux.sh --run-tests`
- macOS: [scripts/build-all-available-presets-macos.sh](scripts/build-all-available-presets-macos.sh)
  - Build: `./scripts/build-all-available-presets-macos.sh`
  - Build + test: `./scripts/build-all-available-presets-macos.sh --run-tests`

Linux/macOS wrappers call the shared runner: [scripts/build-all-available-presets.py](scripts/build-all-available-presets.py)

Note: on Linux/macOS you may need to set the executable bit once:

```bash
chmod +x scripts/build-all-available-presets-linux.sh scripts/build-all-available-presets-macos.sh
```

## CMake Options

Catalyst is modular: you can link individual modules, or link the monolithic umbrella library.

- **Monolithic**: `CATALYST_BUILD_ALL` (default: `ON`)
  - Builds the `catalyst` target (aliases: `catalyst::catalyst`, `catalyst::all`) which links all enabled modules.
- **Modules**: one `CATALYST_BUILD_<MODULE>` switch per module in
  [cmake/CatalystModules.cmake](cmake/CatalystModules.cmake).
  - Implemented modules default to `ON`: `CATALYST_BUILD_AUDIO`, `CATALYST_BUILD_CORE`,
    `CATALYST_BUILD_EVENTS`, `CATALYST_BUILD_INPUT`, `CATALYST_BUILD_LOGGING`, `CATALYST_BUILD_MATH`,
    `CATALYST_BUILD_PLATFORM`, `CATALYST_BUILD_RENDERING`, `CATALYST_BUILD_RESOURCE`,
    `CATALYST_BUILD_SCENE`, `CATALYST_BUILD_TEXT`, `CATALYST_BUILD_UI`.
  - Placeholders default to `OFF`: `CATALYST_BUILD_ANIMATION`, `CATALYST_BUILD_NET`,
    `CATALYST_BUILD_PHYSICS`, `CATALYST_BUILD_UTILS`. They contain a `module_name()` and nothing
    else, and while off their headers are left out of the install.
  - `CATALYST_BUILD_UI_RENDERER` (default: `ON` when both `CATALYST_BUILD_UI` and `CATALYST_BUILD_RENDERING` are)
    builds `catalyst::ui_renderer`, the bridge that draws `catalyst::ui` batches through `catalyst::rendering`.
    It switches itself off when either side is off rather than failing the configure.
  - Any module whose `src/<module>/` directory is not in the checkout defaults to `OFF`, whatever
    the above says. Switching a module off that another one needs fails the configure with one
    sentence naming the switch to flip.
- **Developer mode**: `CATALYST_DEVELOPER_MODE` (default: `ON` when Catalyst is the top-level
  project, `OFF` when it is an `add_subdirectory()` or `FetchContent` dependency). It is the default
  for the three extras below and nothing else.
- **Extras** (each defaults to `CATALYST_DEVELOPER_MODE`, and to `OFF` when its directory is absent):
  - `CATALYST_BUILD_EXAMPLES`
  - `CATALYST_BUILD_TESTS`
  - `CATALYST_BUILD_BENCHMARKS`
- **Other**:
  - `CATALYST_RESOURCE_STB` (default: `ON`) — fetches stb_image at a pinned commit for the source-format
    image decoders. `OFF` makes the resource module dependency-free; `load_image` then reports
    `unsupported_format` for PNG, JPEG and friends but still reads cooked KTX2 and DDS containers.
  - `CATALYST_LOG_COMPILED_LEVEL` (default: `trace`) — the floor below which `catalyst::logging` calls are
    compiled out entirely. One of `trace`, `debug`, `info`, `warn`, `error`, `fatal`, `critical`.

- **Backend selection**:
  - `CATALYST_INPUT_BACKEND` (default: `auto`) values: `auto`, `win32`, `null`
  - `CATALYST_AUDIO_BACKEND` (default: `auto`) values: `auto`, `win32`, `null`
  - `CATALYST_PLATFORM_BACKEND` (default: `auto`) values: `auto`, `win32`, `null`
  - `CATALYST_RENDERING_BACKEND` (default: `auto`) values: `auto`, `d3d12`, `vulkan`, `null`
    - `auto` prefers Vulkan and falls back to `null` when no Vulkan SDK is present, so a fresh clone always
      configures. CMake prints which one it picked. The `null` backend records state and draws nothing — if you
      wanted a renderer that draws, install a Vulkan SDK and reconfigure.
    - `vulkan` needs a Vulkan SDK (1.3 or newer) that CMake's `find_package(Vulkan)` can locate, e.g. via the
      `VULKAN_SDK` environment variable, and a driver exposing Vulkan 1.3. Naming it explicitly makes a missing
      SDK an error rather than a fallback. Shaders are consumed as SPIR-V; the SPIR-V is committed, and
      `scripts/embed_spirv.py` regenerates it with `glslc` after a shader change.
    - `d3d12` is an identity stub whose resource layer is the `null` bookkeeping implementation: it records state
      and draws nothing. It stays opt-in until it is real.

Example: build only a subset of modules (no monolithic target):

```bash
cmake -S . -B build \
    -DCATALYST_BUILD_ALL=OFF \
    -DCATALYST_BUILD_RENDERING=ON \
    -DCATALYST_BUILD_UI=OFF \
    -DCATALYST_BUILD_CORE=ON
cmake --build build --config Release
```

Example: build monolithic Catalyst without audio:

```bash
cmake -S . -B build -DCATALYST_BUILD_AUDIO=OFF
cmake --build build --config Release
```

Example: force specific backends:

```bash
cmake -S . -B build \
  -DCATALYST_INPUT_BACKEND=win32 \
  -DCATALYST_AUDIO_BACKEND=win32 \
  -DCATALYST_PLATFORM_BACKEND=win32 \
  -DCATALYST_RENDERING_BACKEND=d3d12
cmake --build build --config Release
```

## Includes

- Include everything this build contains (umbrella header):
  - `#include <catalyst/catalyst.hpp>`
- Include only a module:
  - `#include <catalyst/rendering/rendering.hpp>`
  - `#include <catalyst/audio/audio.hpp>`

### `<catalyst/config.hpp>`

Generated at configure time, and the way to ask what a given build of Catalyst contains:

```cpp
#include <catalyst/config.hpp>

#if CATALYST_HAS_AUDIO
#  include <catalyst/audio/audio.hpp>
#endif

static_assert(CATALYST_VERSION >= CATALYST_VERSION_ENCODE(0, 1, 0));
```

It defines `CATALYST_HAS_<MODULE>` for every module in the manifest, `CATALYST_VERSION_MAJOR` /
`_MINOR` / `_PATCH` / `_STRING`, the comparable `CATALYST_VERSION` with `CATALYST_VERSION_ENCODE`,
`CATALYST_RENDERING_BACKEND_NAME` (resolved, never `"auto"`) and `CATALYST_HAS_STB_IMAGE`. The
umbrella header uses these to include only what was actually built.

### Namespaces

Everything lives under `catalyst::`, including the math library — `catalyst::math::vec3f`. If you
want the shorter spelling in a translation unit dense with vector maths, ask for it explicitly:

```cpp
#include <catalyst/math/alias.hpp>   // namespace math = catalyst::math;

math::vec3f up{0.0f, 1.0f, 0.0f};
```

Never include that header from a public header of your own: a namespace alias at global scope
reaches everything that includes it, transitively.

## Versioning and compatibility

Catalyst is pre-1.0, and the version number says what to expect:

- A **minor** release (0.1 to 0.2) may change or remove public API. Each such change is listed in
  [CHANGELOG.md](CHANGELOG.md) with what to do about it.
- A **patch** release (0.1.0 to 0.1.1) does not. Code that builds against 0.1.0 builds against
  0.1.x, and the installed package says so: `find_package(Catalyst 0.1)` accepts any 0.1.x and
  refuses 0.2.0 (`SameMinorVersion`).
- From 1.0 the usual rule applies: breaking changes need a new major version.

What the policy covers is what an install contains: the headers under `include/catalyst/`, the
exported `catalyst::<module>` targets, the `find_package` components, and the macros in
`<catalyst/config.hpp>`. What it does not cover:

- Placeholder modules (`animation`, `net`, `physics`, `utils`). They are not installed and may be
  renamed, reshaped or removed in any release.
- Anything in a `detail` namespace or a `detail/` directory, and the backend targets
  (`catalyst::rendering_backend_vulkan` and friends) that appear in the export set only because a
  static library's private links have to.
- The `null` backends' observable values (the size of the null monitor, say). They are stable in
  practice because the tests depend on them, but they are a test fixture, not a promise.

## Documentation

Module documentation lives in the `docs` directory: [audio](docs/audio.md), [input](docs/input.md),
[rendering](docs/rendering.md), [resource](docs/resource.md), [scene](docs/scene.md) and
[ui](docs/ui.md). The remaining modules are documented in their headers only.

The API reference is generated by Doxygen from those headers and published at
[https://colton-mcgraw.github.io/Catalyst/](https://colton-mcgraw.github.io/Catalyst/) on every
push to `main`. To build it locally, with Doxygen installed:

```bash
cmake --build build --target docs   # writes build/docs/html/index.html
```

The `Doxyfile` is generated at configure time from `Doxyfile.in`, so the version it shows is the
one in `CMakeLists.txt`.

## Contributing

Contributions are welcome. [CONTRIBUTING.md](CONTRIBUTING.md) covers the toolchain requirements,
how to build and test, the code style, and what CI checks.

## License

Catalyst is licensed under the MIT License. See the `LICENSE` file for more information.

---
