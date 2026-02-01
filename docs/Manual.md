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

Note that running games made with this engine on `libmali` drivers (which your handheld's OS might use) may cause issues and crashes (for example loading a texture may cause black screen and/or out of memory error) instead prefer to use `panfrost` drivers if your OS provides them. Most of the testing is done on Rocknix which provides an option to change used driver in the settings.

