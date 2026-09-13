# Catalyst v0.1.0

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
| `ui` | Working | Retained tree of styled nodes, CSS-like units, flexbox layout, backend-agnostic draw list. |
| `events` | Working | The bus every other module publishes on. Header-only. |
| `text` | Working | UTF-8 decoding and the scanner the parsers are built on. Header-only. |
| `core` | Minimal | Shared vocabulary. Currently just what more than one module needs. |
| `animation` | **Planned** | Not implemented. The module is a placeholder and defaults to `OFF`. |
| `net` | **Planned** | Not implemented. The module is a placeholder and defaults to `OFF`. |
| `physics` | **Planned** | Not implemented. The module is a placeholder and defaults to `OFF`. |
| `scene` | **Planned** | Not implemented. The module is a placeholder and defaults to `OFF`. |
| `utils` | **Planned** | Not implemented. The module is a placeholder and defaults to `OFF`. |

Not started, and not currently planned for 0.1: scripting, a plugin system, and profiling tools.

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

CMake 3.16 or newer is required. Everything else is optional: see
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

### Multi-compiler builds (Windows)

This repo includes a root [CMakePresets.json](CMakePresets.json) so you can quickly validate builds across multiple compilers.

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

- Build the whole matrix (script):

  ```powershell
  ./scripts/build-all-presets.ps1
  # or: ./scripts/build-all-presets.ps1 -Config Debug
  # or: ./scripts/build-all-presets.ps1 -Config Release
  ```

### Test presets

Test-enabled configure/build presets are provided with the `-tests` suffix.

- Build + run tests (Windows):

  ```powershell
  ./scripts/build-all-presets.ps1 -Tests -RunTests
  ```

- Or run a specific test preset directly:

  ```bash
  # after building one of the *-tests build presets
  ctest --preset clangcl-x64-debug-tests
  ```

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
- **Modules** (all default to `ON`): `CATALYST_BUILD_ANIMATION`, `CATALYST_BUILD_AUDIO`,
  `CATALYST_BUILD_CORE`, `CATALYST_BUILD_EVENTS`, `CATALYST_BUILD_INPUT`, `CATALYST_BUILD_LOGGING`,
  `CATALYST_BUILD_MATH`, `CATALYST_BUILD_NET`, `CATALYST_BUILD_PHYSICS`, `CATALYST_BUILD_PLATFORM`,
  `CATALYST_BUILD_RENDERING`, `CATALYST_BUILD_RESOURCE`, `CATALYST_BUILD_SCENE`,
  `CATALYST_BUILD_TEXT`, `CATALYST_BUILD_UI`, `CATALYST_BUILD_UTILS`
- **Extras**:
  - `CATALYST_BUILD_EXAMPLES` (default: `ON`)
  - `CATALYST_BUILD_TESTS` (default: `ON`)
  - `CATALYST_BUILD_BENCHMARKS` (default: `ON`)
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

Example: build monolithic Catalyst without networking:

```bash
cmake -S . -B build -DCATALYST_BUILD_NET=OFF
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

## Installing and consuming

Catalyst installs, and exports a CMake package, so it does not have to be an `add_subdirectory` of
your tree:

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
and `CATALYST_RENDERING_BACKEND_NAME` so a build can branch on either.

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

It defines `CATALYST_HAS_<MODULE>` for all sixteen modules, `CATALYST_VERSION_MAJOR` / `_MINOR` /
`_PATCH` / `_STRING`, the comparable `CATALYST_VERSION` with `CATALYST_VERSION_ENCODE`,
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

## Documentation

Module documentation lives in the `docs` directory: [audio](docs/audio.md), [input](docs/input.md),
[rendering](docs/rendering.md), [resource](docs/resource.md) and [ui](docs/ui.md). The remaining
modules are documented in their headers only. You can also generate the latest documentation using Doxygen:

```bash
doxygen Doxyfile
```

This will create HTML documentation in the `docs/html` directory.

You can also view the documentation online at [https://colton-mcgraw.github.io/Catalyst/docs](https://colton-mcgraw.github.io/Catalyst/docs).

## Contributing

Contributions are welcome. [CONTRIBUTING.md](CONTRIBUTING.md) covers the toolchain requirements,
how to build and test, the code style, and what CI checks.

## License

Catalyst is licensed under the MIT License. See the `LICENSE` file for more information.

--
