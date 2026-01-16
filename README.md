# tiny engine

Tiny 3D game engine for making small low-poly games.

Supported platforms: Windows (x64), Linux (x64 and ARM64).

# Roadmap

- [X] Simple ECS
- [ ] Config management (progress, settings, etc.)
- [ ] Type reflection
- [ ] Forward renderer using OpenGL ES 2.0
- [ ] GLTF/GLB import
- [ ] GUI
- [ ] Audio
- [ ] Simple editor
- [ ] Simple physics engine
- [ ] Skeletal animations
- [ ] Minimal scripting
- [ ] Shadow mapping 
- [ ] Particle effects

# Documentation

Documentation for this engine consists of 2 parts: API reference (generated from code comments) and the manual (at `docs/Manual.md`), Doxygen is used to generate documentation in the HTML format, it includes both API reference and the manual (copied from `docs/Manual.md`).

If you're are game developer you generally don't need Doxygen, the only thing that you need is the manual, you can read it at `docs/Manual.md`.

Because the Doxygen is configured to turn warnings into errors any missing documentation will make Doxygen fail with an error. We don't run Doxygen locally but instead have it in the CI to monitor in case we missed some docs. If you want to generate documentation locally you need to execute the `doxygen` command while being in the `docs` directory. Generated documentation will be located at `docs/gen/html`, open the `index.html` file from this directory to view the documentation.

# Prerequisites

- Compiler that supports C99
- [CMake](https://cmake.org/download/)

Optional but highly recommended:
- [LLVM](https://github.com/llvm/llvm-project/releases/latest) for clang-format

