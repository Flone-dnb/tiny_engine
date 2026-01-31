# Manual

This is a manual - a step-by-step guide to introduce you to various aspects of the engine. This manual will not explain every piece of code, only some high-level entities and their usage, more specific documentation can be always found in the struct/function/variable documentation in the source code, every code entity is documented so you should not get lost.

# Project manager

TODO

# Thread safety

The engine is single threaded and expects all functions to be executed in a single (main) thread. Only game resource loading (deserialization) is asynchronous (so you can show a loading screen while loading game resources).

# Memory leak checks

In debug builds the engine has memory leak checks enabled (thanks to CRT library on Windows and ASAN on Linux). Look for the output/debugger output tab of your IDE after running your project. If any leaks occurred it should print about them. You can test whether the memory leak checks are enabled or not by doing something like this:
```C
malloc(1);
// not using `free`
```
run your program that has this code and after your program is finished you should see a message about the memory leak in the output/debugger output tab of your IDE.

# Naming

Engine types have the `te_` prefix (tiny engine). Functions have filename prefix so for example if you include `renderer.h` all of its functions will start with the `renderer_` prefix.

# Core objects

When the game is running the object ownership structure looks like this:

- `te_window` (i.e. game window) owns:
    - `te_game_manager` which manages core engine systems like:
        - `te_renderer`
        - `te_world` owns game entities, you can think of this as a game level
            - your game objects are here
        - etc.

This means that for example when you query `te_window` pointer in some of your game objects you should not free/destroy received pointer, that pointer will be valid during the whole lifetime of your game object.

# Managing pointers

When working with the engine you would often see the following approach: in order to create some engine object use `..._create()` function and later `..._destroy()` to destroy it. For example in order to create a camera (game object):

```C
#include "game/camera.h"

te_camera* camera = camera_create();
// later ...
camera_destroy(camera);
```

When the engine returns a pointer to you (like in the example above) often you need to make sure to free/destroy the pointer but in some cases you should not do that for example when the engine returns pointer to the game's window, in these cases the documentation for the function will specifically state that you should not free/destroy returned pointer so make sure to read the docs on the functions you are using.

# Game world

Create a new game world (level) using the following code:

```C
te_world* game_world = game_manager_create_world(game_manager, "game");

// ... and later don't forget to:
game_manager_destroy_world(game_manager, game_world);
```

Game world is a container for game objects.

# Game objects

You can find all available game objects in `src/engine_lib/include/game`.

The camera is a game object that's needed to view the world, here's an example of creating such object:

```C
te_camera* camera = camera_create();
world_spawn_camera(game_world, camera); // spawn first
world_set_active_camera(game_world, camera); // then set active

// ... and later:
camera_destroy(camera);
```

Other game objects include things like models (AKA meshes), their usage is similar.

# Texture import

There's no texture import, just copy your image somewhere inside of the `res` directory.

In order to use the imported texture (for example on a model) you need to assign it using the editor or using the code like so:

```C
model_set_texture(model, "game/texture/sometex.png"); // located at `res/game/...`
```

When you import a GLTF/GLB file textures will be automatically imported (copied) to the `res` directory and the connection between imported meshes and their assigned textures will be saved in the imported files.

