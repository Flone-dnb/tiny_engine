#include "io/filesystem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WIN32)
#define NOMINMAX
#include <windows.h>
#define mkdir(dir, mode) _mkdir(dir)
#elif __linux__
#include <sys/stat.h>
#else
#error "unsupported OS"
#endif

void
filesystem_ensure_dirs_exist(const char* file_path) {
    const char pathSeparator =
#if defined(WIN32)
        '\\';
#else
        '/';
#endif

    char* dir_path = malloc(strlen(file_path) + 1);

    char* next_sep = strchr(file_path, pathSeparator);
    while (next_sep != NULL) {
        long dir_path_len = next_sep - file_path;
        memcpy(dir_path, file_path, (unsigned long)dir_path_len);
        dir_path[dir_path_len] = 0;

        if (!filesystem_does_path_exists(dir_path)) {
            mkdir(dir_path, 0755);
        }

        next_sep = strchr(next_sep + 1, pathSeparator);
    }

    free(dir_path);
}

bool
filesystem_does_path_exists(const char* path) {
#if defined(WIN32)
    DWORD attributes = GetFileAttributes(path);
    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
#elif __linux__
    struct stat buffer;
    return stat(path, &buffer) == 0;
#else
#error "unsupported OS"
#endif
}

void
filesystem_remove_file(const char* path) {
#if defined(WIN32)
    DeleteFile(path);
#elif __linux__
    remove(path);
#else
#error "unsupported OS"
#endif
}
