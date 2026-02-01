//
// This file provides cross-platform filesystem functions.
//

#pragma once

#include <stdbool.h>

// Recursively creates directories for the specified path (if directories did not existed before).
void filesystem_ensure_dirs_exist(const char* path);

// Checks if the specified path exists.
bool filesystem_does_path_exists(const char* path);

// Deletes a file at the specified path.
void filesystem_remove_file(const char* path);
