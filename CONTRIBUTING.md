# Contributing to Catalyst

Thanks for taking an interest. This file covers what you need to build the project, what CI will
check, and the conventions the code follows.

## Toolchain

Catalyst is C++23 and uses two features that are only just becoming widely available: deducing this
and `std::expected`. That makes the supported list short:

| Toolchain | Status |
| --- | --- |
| GCC 14 or newer | Supported |
| Clang 19 or newer, against libstdc++ 14 or newer | Supported |
| MSVC 19.40 or newer (Visual Studio 2022 17.10+) | Supported |
| GCC 13 | No deducing this |
| Clang 18 | libstdc++ gates `<expected>` on `__cpp_concepts >= 202002L`, which Clang reports only from 19 |
| libc++ | Leaves the floating-point `std::from_chars` overloads deleted, which the JSON and CSV parsers need |

CMake checks both features at configure time (`cmake/CatalystCompilerSupport.cmake`) and stops with
a message naming what is missing, rather than letting the build fail hundreds of template errors
later. If you are on Ubuntu 24.04 the default `g++` is 13, so install `g++-14` and point CMake at
it.

CMake 3.23 or newer. That is the floor for the project, the presets file, and the consumer fixture
alike; if you raise it, raise all three.

## Building and testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCATALYST_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

A checkout configured directly is in developer mode (`CATALYST_DEVELOPER_MODE=ON`), so the tests,
examples and benchmarks are on already; `-DCATALYST_BUILD_TESTS=ON` above just makes it explicit.
A project that pulls Catalyst in as a dependency gets none of the three unless it asks.

`CATALYST_RENDERING_BACKEND` defaults to `auto`, which prefers Vulkan and falls back to the `null`
backend when no SDK is present. The test suite runs against `null`, so it needs no GPU and no SDK.
The `platform` suites additionally need `-DCATALYST_PLATFORM_BACKEND=null`, because against Win32
they would open real windows; on Linux that is already the default.

Before opening a pull request, run the checks CI runs:

```bash
# format
clang-format --dry-run --Werror $(git ls-files '*.hpp' '*.cpp')

# tests, on a supported compiler
ctest --test-dir build --output-on-failure

# and, for anything touching lifetimes, coroutines or threading
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCATALYST_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

The sanitisers are worth the minute they cost. They are how a coroutine lambda that had been
reading a destroyed closure — tolerated by GCC, a segfault under Clang — was found.

For anything touching CMake, also run the consumer fixture, which is what proves an installed
Catalyst can be found and linked from a separate project:

```bash
cmake -S . -B build-install -DCMAKE_INSTALL_PREFIX=$PWD/install-prefix
cmake --build build-install --parallel
cmake --install build-install
cmake -S tests/consumer -B build-consumer -DCMAKE_PREFIX_PATH=$PWD/install-prefix
cmake --build build-consumer
ctest --test-dir build-consumer --output-on-failure
```

## What CI checks

`.github/workflows/ci.yml` runs on every push and pull request:

- **build** — configure, build and `ctest` on Linux/GCC 14, Linux/Clang 20 and Windows/MSVC. A
  macOS/Apple Clang job runs too but is non-blocking, because libc++ cannot build the library yet.
- **vulkan** — compiles and links the Vulkan backend on Linux. It is not run: the committed SPIR-V
  means no shader compiler is needed, but creating a device needs a GPU the runner does not have.
- **configurations** — builds seven module subsets, because the module switches are an advertised
  feature and something has to exercise them. It then checks that switching off a module another
  one needs fails with a readable sentence, builds a sparse checkout of two modules (see
  *Fetching only what you need* in the README), and installs Catalyst and builds `tests/consumer/`
  against it — including that requiring a component the install lacks fails, naming what it has.

## Code style

`.clang-format` is authoritative — run it, don't hand-format. In short: Allman braces, four-space
indent, indented namespace bodies, 120 columns, `const char *name` with the pointer bound to the
name, `<catalyst/...>` includes before the standard library.

Public headers include each other with angle brackets — `#include <catalyst/math/vector.hpp>`, never
`#include "vector.hpp"`. Angle brackets are what an installed header needs, since a consumer reaches
it through the include path rather than from the directory next to it, and it is also the only
spelling that resolves unambiguously where several modules have a `win32/` subdirectory of their own.
Quotes stay for headers that are private to `src/` and for test helpers: those are genuinely relative
to the file including them and are not installed.

Beyond formatting, the convention that matters most here is **comments explain why, not what**. The
code is readable; what it cannot tell you is which alternative was rejected and what went wrong last
time. Comments in this codebase carry that, and a patch is more likely to be merged quickly if it
does the same.

Public headers get Doxygen (`@brief`, `@details`, `@param`, `@return`). New files start with the
SPDX header below. The tree is not there yet — it currently has three states: 99 files with this
block, 161 with an older one-line `License: MIT (see LICENSE).` inside the doc comment, and 92 (most
of `math/`, plus the examples and benchmarks) with no marker at all. Bringing them to one form is
worth doing; until then, follow this for anything new rather than matching the file next to it:

```cpp
/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief One sentence on what this file is.
 */
```

## Module layout

Public headers live in `include/catalyst/<module>/`, implementation in `src/<module>/`. Each module
is a CMake target with a `CATALYST_BUILD_<MODULE>` switch and a `catalyst::<module>` alias.

Every module is declared once, in `cmake/CatalystModules.cmake`. The declaration owns the
module's switch and its default, its source directory, its dependencies and one line of
description; the root options, the `src/` build order, the monolithic link list, the generated
`<catalyst/config.hpp>`, the install export, the tests tree and the sparse-checkout helper all
read the list from there. If you add a module, that file is the first edit; the second is the
`#if CATALYST_HAS_<MODULE>` block in `include/catalyst/catalyst.hpp`, and the configure warns if
you forget it. Then the README status table and `CHANGELOG.md`.

Modules are deliberately independent. A module that needs another declares it — `PUBLIC` when a
public header includes it, `PRIVATE` when only a `.cpp` does — and, where the dependency is
structural, lists it under `DEPENDS` in the manifest so that `catalyst_require_modules(<module>)`
at the top of its `CMakeLists.txt` turns switching the wrong thing off into one clear sentence
instead of a wall of missing includes. The bar for `DEPENDS` is that the module does not compile
without the other, on any platform; a dependency used only from a `.cpp` on Windows still counts.

Four modules are foundations, declared first and header-only: `core` (shared vocabulary),
`events` (the bus every module publishes on), `text` (the scanner the parsers use) and `math`.

Backends live in a subdirectory per backend (`src/rendering/vulkan/`, `src/audio/wasapi/`) behind a
`CATALYST_<MODULE>_BACKEND` option, and every module has a `null` backend so it builds, links and
tests on every platform.

The build tolerates a sparse checkout: a module whose `src/` directory is absent defaults to
`OFF`, and so do the examples, tests and benchmarks when their directories are. Keep it that way —
guard any new top-level directory the same way the existing ones are in the root `CMakeLists.txt`.

## Placeholder modules

`animation`, `net`, `physics` and `utils` are a single `module_name()` function each. They are
marked `PLACEHOLDER` in the manifest, default to `OFF`, are left out of the install while off, and
the README marks them planned and outside the compatibility policy. If you want to implement one,
that is a good place to start — say so in an issue first so two people don't write it twice, and
drop the `PLACEHOLDER` flag when there is something to install.

## Pull requests

- One concern per pull request. A formatting sweep and a behaviour change in the same diff is a diff
  nobody can review.
- Tests for anything with behaviour. `tests/<module>/` shows the pattern; the harness is
  `tests/test_common.hpp` and its `CT_REQUIRE`.
- Say what you verified and on which toolchain.
- Update the README or `docs/` when you change something they describe, and add a line to
  `CHANGELOG.md` under *Unreleased* for anything a user of the library would notice.

## License

Catalyst is MIT. Contributions are accepted under the same license.
