//
// This file provides cross-platform filesystem functions.
//

#pragma once

#include <stdbool.h>

// Recursively creates directories for the specified path (if directories did not existed before).
void filesystem_ensure_dirs_exist(const char* path);

bool filesystem_does_path_exists(const char* path);

void filesystem_remove_file(const char* path);

void filesystem_rename_file(const char* old_path, const char* new_path);

void filesystem_copy_file(const char* src, const char* dst);