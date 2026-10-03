# Changelog

All notable changes to Catalyst are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the version numbers follow the
policy in the README under *Versioning and compatibility*: while Catalyst is pre-1.0, a minor
release may break the API and a patch release may not.

## [Unreleased]

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

## [0.1.0]

Initial development release: rendering, resource, audio, input, logging, platform, math, ui,
scene, events and text modules, with Win32 and Vulkan backends and null backends for every module.
