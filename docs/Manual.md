# Manual

This is a manual - a step-by-step guide to introduce you to various aspects of the engine. This manual will not explain every piece of code, only some high-level entities and their usage, more specific documentation can be always found in the source code comments.

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

# Config files

`te_config` provides functionality for reading and writing config files, these are human-readable text files in a custom format which is similar to INI or TOML but way simpler (see `config.h` for more info).

You can use `te_config` for creating game save files and various config files. Game levels (worlds) are also saved using `te_config` and are also stored in this human-readable format. There's no special file extension for such config files and it does not matter which you use, you can use the ".txt" extension for example. Note that it might be not a good idea to use ".ini" or ".toml" extensions because `te_config` format might not be compatible to INI or TOML (see `config.h` for details), although there still might be good reasons to use such file extensions anyway (for example for text editors to apply syntax highlighting), it's up to you to decide.

# Game world

Create a new game world (level) using the following code:

```C
te_world* game_world = game_manager_create_world(game_manager, "game");

// ... and later don't forget to:
game_manager_destroy_world(game_manager, game_world);
```

Game world is a container for game objects (described in the next section).

In order to save the world use `world_save_to_file` and in order to add game objects from a file to a world use `world_add_from_file`. Using these functions you can implement sublevels and prefabs (sort of): create a small scene in the editor (or in the code) then save it, then add this world (sublevel) to some other world (main level) using the "add from file" function.

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

# Debug tools

Debug tools include things like `debug_drawer` (for rendering debug shapes) and `debug_console` (for registering new dev cheat commands and performance stats). In order to show/hide `debug_console` press the tilde (~) button on your keyboard.

Debug console can also show various statistics such as FPS, RAM usage, number of drawn meshes, various GPU metrics and etc. In order to view such statistics use the commands `show_stats` and `hide_stats`. Note that with `show_stats` you might want to also use the command `set_fps_limit 0` to make sure your GPU is running at max power (not being limited).

Debug tools are disabled in the "Release" build mode but if you need them in your release builds you can enable them if you pass -DENGINE_FORCE_ENABLE_DEBUG_TOOLS=ON while configuring cmake, cmake will then print a warning that you enabled debug tools in release build.

Note that in case you created a custom game object and want it to (for example) draw something when used in the editor you can use `ifdef ENGINE_EDITOR` macro for code that will only run in the editor.

In case your only input device is a gamepad you can click both the "start" button and the "menu" button (sometimes called the "back" button) to toggle debug stats command and disable fps limit while the stats are visible.

# Texture import

There's no texture import, just copy your image somewhere inside of the `res` directory.

In order to use the imported texture (for example on a model) you need to assign it using the editor or using the code like so:

```C
model_set_texture(model, "game/texture/sometex.png"); // located at `res/game/...`
```

When you import a GLTF/GLB file textures will be automatically imported (copied) to the `res` directory and the connection between imported meshes and their assigned textures will be saved in the imported files.

# Loading font

In order to load a font you need to have a .ttf file to load (there is a default .ttf in the res/engine/font). To load the font file you need to do the following at the start of your game:

```Cpp
void on_game_started(void* game_instance, te_game_manager* game_manager) {
    te_renderer* renderer = game_manager_get_renderer(game_manager);
    te_font_manager* font_manager = renderer_get_font_manager(renderer);
    font_manager_load_font(font_manager, "game/myfont.ttf");  // located at "res/game/myfont.ttf"

    // ... create world, game objects, etc. ...
}
```

# Widgets

UI elements are called widgets, you can find them in the "src/engine_lib/include/widgets" directory.

All widgets contain a `te_widget` object inside of them, it implement base widget functionality such as: position, size, being able to attach to other widgets (thus creating a widget hierarchy) and so on. You can get `te_widget*` by using "..._get_widget" function.

In order to display a widget (and all of its attached child widgets) you need to spawn the root widget in a world (same as with models). You can also spawn widget by attaching a non-spawned widget to a spawned widget using the `set_parent` function.

# Reflection

`type_database.h` is used for registering a type info, you need to define a unique string (ID) of your type and then register it in the type database, here's a short example from `te_model` type:

```C
void model_get_type_id(void) {
    return "model";
}

void model_register_type(void) {
    te_type_info* info = type_info_create(model_get_type_id());
    type_info_add_vec3_variable(info, "position", model_set_position, model_get_position);
    type_info_add_string_variable(info, "texture", model_set_texture, model_get_texture);
    type_info_add_vec2_variable(info, "texture_tiling", model_set_texture_tiling, model_get_texture_tiling);

    type_database_register_type(info);
}
```

Later in order to get type info all you need is this "type ID" string and a `void*` pointer to an object:

```C
const te_type_info* info = type_database_get_type_info(some_id);
if (info == NULL) {
    TODO; // not registered
}

// use setters/getters from `info`
```

# Building your game for retro handhelds (ARM64 Linux devices)

## Setup

This section describes the build process for such devices as Anbernic RG35XX H or similar.

- Based on https://github.com/Cebion/Portmaster_builds

The section will describe commands for WSL2 Ubuntu 24.04.1 LTS (for Windows users, Linux users you know what to do):
```
sudo apt update && sudo apt upgrade -y
sudo reboot now
```
Wait for console to close, then in Windows console:
```
wsl --shutdown
```
Then back in Ubuntu:
```
sudo apt install -y build-essential binfmt-support daemonize libarchive-tools qemu-system qemu-user qemu-user-static gcc-aarch64-linux-gnu
wget "https://cloud-images.ubuntu.com/releases/24.04/release/ubuntu-24.04-server-cloudimg-arm64-root.tar.xz"
mkdir arm64ubuntu
sudo bsdtar -xpf ubuntu-24.04-server-cloudimg-arm64-root.tar.xz -C arm64ubuntu

sudo cp /usr/bin/qemu-aarch64-static arm64ubuntu/usr/bin
sudo daemonize /usr/bin/unshare -fp --mount-proc /lib/systemd/systemd --system-unit=basic.target
sudo mount -o bind /dev arm64ubuntu/dev
sudo chroot arm64ubuntu qemu-aarch64-static /bin/bash
rm /etc/resolv.conf
echo 'nameserver 8.8.8.8' > /etc/resolv.conf
exit
mkdir -p arm64ubuntu/tmp/.X11-unix

echo '#!/bin/bash' > chroot.sh
echo 'sudo daemonize /usr/bin/unshare -fp --mount-proc /lib/systemd/systemd --system-unit=basic.target' >> chroot.sh
echo 'sudo mount -o bind /proc/ arm64ubuntu/proc/' >> chroot.sh
echo 'sudo mount --rbind /dev/ arm64ubuntu/dev/' >> chroot.sh
echo 'sudo mount -o bind /tmp/.X11-unix arm64ubuntu/tmp/.X11-unix' >> chroot.sh
echo 'sudo chroot arm64ubuntu qemu-aarch64-static /bin/bash' >> chroot.sh
chmod +x chroot.sh
sudo ./chroot.sh
```
Try `sudo apt update && sudo apt upgrade -y` if you get an error `sudo: unable to resolve host ...` write the hostname that you got in that message in the file `/etc/hostname` (replace the old one) and in the file `/etc/hosts` under the localhost string (use the same 127.0.0.1 address).
```
sudo apt update && sudo apt upgrade -y
sudo apt install --no-install-recommends build-essential git wget libdrm-dev libopenal-dev premake4 autoconf libevdev-dev pkg-config zlib1g-dev cmake cmake-data libarchive13 libcurl4 libfreetype6-dev librhash0 libuv1 libgbm-dev clang libvorbis-dev libflac-dev
```

Then install SDL dependencies: https://github.com/libsdl-org/SDL/blob/main/docs/README-linux.md#build-dependencies

## Steps for each release of your game

Copy your game using the Windows explorer into a new directory at ~/arm64ubuntu/tmp/game (directory inside of the chroot). Then back into the WSL:
```
sudo ./chroot.sh
cd tmp/game
mkdir build
cd build
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --target <game_target_name> --config=Release --parallel
```
Then copy the resulting binary (from `build/OUTPUT/game`) to your ARM64 Linux device. We don't worry about installing SDL and other libraries because we link SDL and other libraries statically. Inside of your ARM64 Linux device launch the game using some file explorer or a console.

