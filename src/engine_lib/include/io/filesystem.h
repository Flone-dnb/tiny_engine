#pragma once
#include <stdbool.h>

typedef struct te_filesystem_entry {
    // Name of a file/directory.
    char* name;

    // `true` if it's a directory.
    bool is_dir;
} te_filesystem_entry;

// ------------------------------------------------------------------------------------------------

bool filesystem_does_path_exists(const char* path);

// Recursively creates directories for the specified path (if directories did not existed before).
void filesystem_ensure_dirs_exist(const char* path);
void filesystem_create_directory(const char* path);

void filesystem_remove_file(const char* path);
void filesystem_rename_file(const char* old_path, const char* new_path);
void filesystem_copy_file(const char* src, const char* dst);

// ------------------------------------------------------------------------------------------------

// Returns pointer to the first character of the filename from the specified path string.
// Also works for directories.
// Returns NULL if filename not found.
const char*
filesystem_find_filename(const char* path, bool include_extension, unsigned int* ret_len);

// ------------------------------------------------------------------------------------------------

// Returns a new string that you must free.
char* filesystem_convert_path_to_absolute(const char* src);

// Converts the specified path to be relative to the `res` directory.
// Returns NULL if unable to convert, otherwise a new string that you must free.
char* filesystem_convert_path_to_relative(const char* src);

// ------------------------------------------------------------------------------------------------

// Takes a path relative to the `res` directory and appends "res/" before it.
// Specify NULL as `ret_strlen` to ignore returned string strlen.
// You must free returned pointer.
char* filesystem_prepend_res_to_path(const char* relative_path, unsigned int* ret_strlen);

// Appends path to an existing one, adds a slash character between the paths if needed.
// Specify 0 as `path_len` and/or `add_len` to determine automatically.
// Specify NULL as `ret_strlen` to ignore returned string strlen.
// You must free returned pointer.
char* filesystem_append_path(
    const char* path, unsigned int path_len, const char* add, unsigned int add_len,
    unsigned int* ret_strlen);
// Same as above but appends `extension` string to the `add` string (without adding a slash).
char* filesystem_append_path_ext(
    const char* path, unsigned int path_len, const char* add, unsigned int add_len,
    const char* extension, unsigned int extension_len, unsigned int* ret_strlen);

// ------------------------------------------------------------------------------------------------

// Returns all filesystem entries (files and directories) in the specified directory (not recursive).
// You must free returned array and entry names.
te_filesystem_entry*
filesystem_list_directory(const char* path_to_dir, unsigned int* entry_count);
