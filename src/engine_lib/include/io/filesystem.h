//
// This file provides cross-platform filesystem functions.
//

#pragma once

#include <stdbool.h>

typedef struct te_filesystem_entry {
    // Name of a file/directory.
    char* name;

    // `true` if it's a directory.
    bool is_dir;
} te_filesystem_entry;

// Recursively creates directories for the specified path (if directories did not existed before).
void filesystem_ensure_dirs_exist(const char* path);

bool filesystem_does_path_exists(const char* path);

void filesystem_remove_file(const char* path);

void filesystem_rename_file(const char* old_path, const char* new_path);

void filesystem_copy_file(const char* src, const char* dst);

// Returns a new string that you must free.
char* filesystem_convert_path_to_absolute(const char* src);

// Converts the specified path to be relative to the `res` directory.
// Returns NULL if unable to convert, otherwise a new string that you must free.
char* filesystem_convert_path_to_relative(const char* src);

// Returns all filesystem entries (files and directories) in the specified directory (not recursive).
// You must free returned array and entry names.
te_filesystem_entry*
filesystem_list_directory(const char* path_to_dir, unsigned int* entry_count);
