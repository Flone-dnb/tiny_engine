#pragma once

#include <stdbool.h>

/**
 * Recursively creates directories (if not existed before).
 *
 * @param path Path to check.
 */
void filesystem_ensure_dirs_exist(const char* path);

/**
 * Checks if the specified path exists.
 *
 * @param path Path to check.
 *
 * @return `true` if exists.
 */
bool filesystem_does_path_exists(const char* path);

/**
 * Deletes the file at the specified path.
 *
 * @param path Path to file.
 */
void filesystem_remove_file(const char* path);
