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

CMake 3.16 or newer.

## Building and testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCATALYST_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`CATALYST_RENDERING_BACKEND` defaults to `auto`, which prefers Vulkan and falls back to the `null`
backend when no SDK is present. The test suite runs against `null`, so it needs no GPU and no SDK.

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

## What CI checks

`.github/workflows/ci.yml` runs on every push and pull request:

- **build** — configure, build and `ctest` on Linux/GCC 14, Linux/Clang 20 and Windows/MSVC. A
  macOS/Apple Clang job runs too but is non-blocking, because libc++ cannot build the library yet.
- **vulkan** — compiles and links the Vulkan backend on Linux. It is not run: the committed SPIR-V
  means no shader compiler is needed, but creating a device needs a GPU the runner does not have.
- **configurations** — configures five module subsets, because the module switches are an
  advertised feature and something has to exercise them.

## Code style

`.clang-format` is authoritative — run it, don't hand-format. In short: Allman braces, four-space
indent, indented namespace bodies, 120 columns, `const char *name` with the pointer bound to the
name, `<catalyst/...>` includes before the standard library.

Beyond formatting, the convention that matters most here is **comments explain why, not what**. The
code is readable; what it cannot tell you is which alternative was rejected and what went wrong last
time. Comments in this codebase carry that, and a patch is more likely to be merged quickly if it
does the same.

Public headers get Doxygen (`@brief`, `@details`, `@param`, `@return`). Every file starts with the
SPDX header:

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

Modules are deliberately independent. A module that needs another declares it — `PUBLIC` when a
public header includes it, `PRIVATE` when only a `.cpp` does — and, where the dependency is
structural, calls `catalyst_require_modules()` so switching the wrong thing off produces one clear
sentence instead of a wall of missing includes.

Four modules are foundations, added before the rest in `src/CMakeLists.txt` and header-only:
`core` (shared vocabulary), `events` (the bus every module publishes on), `text` (the scanner the
parsers use) and `math`.

Backends live in a subdirectory per backend (`src/rendering/vulkan/`, `src/audio/wasapi/`) behind a
`CATALYST_<MODULE>_BACKEND` option, and every module has a `null` backend so it builds, links and
tests on every platform.

## Placeholder modules

`animation`, `net`, `physics`, `scene` and `utils` are a single `module_name()` function each. They
default to `OFF` and the README marks them planned. If you want to implement one, that is a good
place to start — say so in an issue first so two people don't write it twice.

## Pull requests

- One concern per pull request. A formatting sweep and a behaviour change in the same diff is a diff
  nobody can review.
- Tests for anything with behaviour. `tests/<module>/` shows the pattern; the harness is
  `tests/test_common.hpp` and its `CT_REQUIRE`.
- Say what you verified and on which toolchain.
- Update the README or `docs/` when you change something they describe.

## License

Catalyst is MIT. Contributions are accepted under the same license.
