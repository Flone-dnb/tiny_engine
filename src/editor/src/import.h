#pragma once

#include <stdbool.h>

struct te_game_manager;

/* imports scene from GLTF/GLB file and create a new world file with imported objects
 * specify path to the file and a path to a directory (relative to the `res` directory) to import the files
 * returns `true` if imported successfully */
bool import_file_as_world(
    struct te_game_manager* game_manager, const char* path_to_file,
    const char* relative_path_to_dir);
