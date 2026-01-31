# Catalyst v0.1.0

Catalyst is a high-performance rendering framework for C++ designed to simplify the development of applications. It provides a robust set of features, including 2D and 3D rendering, UI components, and cross-platform support.

The goal of Catalyst is to offer developers a powerful yet easy-to-use toolkit for building visually rich applications. Whether you're creating games, simulations, desktop applications, or data visualization tools, Catalyst has you covered.

## Features

- High-performance 2D and 3D rendering
- Easy-to-use UI components
- Cross-platform support (Windows, macOS, Linux, iOS, Android)
- Modular architecture for easy extension
- Comprehensive documentation and examples

## Getting Started

To get started with Catalyst, follow these steps:

1. Clone the repository:

   ```bash
   git clone https://github.com/yourusername/Catalyst.git
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

4. Run the example application:

    ```bash
    ./build/examples/sandbox/Release/catalyst_sandbox.exe
    ```

## CMake Options

Catalyst is modular: you can link individual modules, or link the monolithic umbrella library.

- **Monolithic**: `CATALYST_BUILD_ALL` (default: `ON`)
  - Builds the `catalyst` target (aliases: `catalyst::catalyst`, `catalyst::all`) which links all enabled modules.
- **Modules** (all default to `ON`):
  - `CATALYST_BUILD_ANIMATION`
  - `CATALYST_BUILD_AUDIO`
  - `CATALYST_BUILD_CORE`
  - `CATALYST_BUILD_INPUT`
  - `CATALYST_BUILD_MATH`
  - `CATALYST_BUILD_NET`
  - `CATALYST_BUILD_PHYSICS`
  - `CATALYST_BUILD_PLATFORM`
  - `CATALYST_BUILD_RENDERING`
  - `CATALYST_BUILD_RESOURCE`
  - `CATALYST_BUILD_SCENE`
  - `CATALYST_BUILD_UI`
  - `CATALYST_BUILD_UTILS`
- **Extras**:
  - `CATALYST_BUILD_EXAMPLES` (default: `ON`)
  - `CATALYST_BUILD_TESTS` (default: `OFF`)

- **Backend selection**:
  - `CATALYST_INPUT_BACKEND` (default: `auto`) values: `auto`, `win32`, `null`
  - `CATALYST_AUDIO_BACKEND` (default: `auto`) values: `auto`, `win32`, `null`
  - `CATALYST_PLATFORM_BACKEND` (default: `auto`) values: `auto`, `win32`, `null`
  - `CATALYST_RENDERING_BACKEND` (default: `auto`) values: `auto`, `d3d12`, `vulkan`, `null`

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

## Includes

- Include everything (umbrella header):
  - `#include <catalyst/catalyst.hpp>`
- Include only a module:
  - `#include <catalyst/physics/physics.hpp>`
  - `#include <catalyst/rendering/rendering.hpp>`

## Components

Catalyst is composed of several key components that can be used independently or together:

- **Renderer**: Handles all rendering operations, supporting both 2D and 3D graphics.
- **UI System**: Provides a set of UI components for building user interfaces.
- **Input Handling**: Manages input from various devices, including keyboard, mouse, and touch.
- **Math Library**: Offers a collection of mathematical functions and data structures commonly used in graphics programming.
- **Utilities**: A set of helper functions and classes to simplify common tasks.
- **Platform Abstraction**: Ensures compatibility across different operating systems and hardware.
- **Resource Management**: Efficiently loads and manages assets such as textures, models, and shaders.
- **Scene Graph**: Organizes and manages the hierarchical structure of objects in a scene.
- **Animation System**: Provides tools for creating and managing animations for objects within the framework.
- **Networking Module**: Enables network communication for multiplayer applications or data synchronization.
- **Audio System**: Supports audio playback and manipulation for immersive experiences.
- **Scripting Support**: Integrates scripting languages for dynamic behavior and rapid prototyping.
- **Physics Engine**: Offers basic physics simulation capabilities for realistic object interactions.
- **Plugin System**: Allows for easy extension of the framework through plugins.
- **Debugging Tools**: Includes tools for profiling and debugging applications built with Catalyst.
- **Documentation Generator**: Facilitates the creation of documentation for projects using Catalyst.
- **Testing Framework**: Provides a suite of tools for unit testing and integration testing of applications.

## Documentation

Comprehensive documentation is available in the `docs` directory. You can also generate the latest documentation using Doxygen:

```bash
doxygen Doxyfile
```

This will create HTML documentation in the `docs/html` directory.

## Contributing

Contributions are welcome! Please read the `CONTRIBUTING.md` file for guidelines on how to contribute to the project.

## License

Catalyst is licensed under the MIT License. See the `LICENSE` file for more information.

--
