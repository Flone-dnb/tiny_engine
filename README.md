# tiny engine

Tiny 3D game engine for making small games.

Supported platforms:

- Windows (Windows 98 and above (x32, x64))
- Linux (x32, x64)

# Features

- [X] Renderer using OpenGL 1.1
- [X] Game save/config management
- [X] GUI
- [X] Type reflection
- [X] Simple editor
- [X] GLTF/GLB import
- [X] Audio
- [X] Skeletal animations
- [X] Scene animations (cutscene editor)
- [X] Particle effects

These features are enough for me to make games but there are some additional "nice to have" features planned:

- [ ] Simple physics engine

# Documentation

You can read the manual from `docs/Manual.md` which gives a general overview of the engine, also in the code a lot of functions have documentation comments in the header files.

# Build

- you need a compiler that supports C89/C90 (which is pretty much any C/C++ compiler nowadays)
- to build the engine you need to use our custom (but simple) build system, you can find more information about it at `tools/tb`


