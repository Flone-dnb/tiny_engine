#include <io/filesystem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io/log.h>

#if defined(WIN32)
#define NOMINMAX
#include <direct.h>
#include <windows.h>
#define mkdir(dir, mode) _mkdir(dir)
#elif __linux__
#include <sys/stat.h>
#else
#error "unsupported OS"
#endif

void
filesystem_ensure_dirs_exist(const char* file_path) {
    const char pathSeparator = '/'; // we convert \\ to / on windows

    char* dir_path = malloc(strlen(file_path) + 1);

    char* next_sep = strchr(file_path, pathSeparator);
    while (next_sep != NULL) {
        const size_t dir_path_len = (size_t)(next_sep - file_path);
        memcpy(dir_path, file_path, dir_path_len);
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
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        DWORD attrib = GetFileAttributes(path);
        return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
    } else {
        fclose(fp);
        return true;
    }
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

void
filesystem_rename_file(const char* old_path, const char* new_path) {
    if (rename(old_path, new_path) != 0) {
        log_error_fmt("failed to rename file from \"%s\" to \"%s\"", old_path, new_path);
        abort();
    }
}

void
filesystem_copy_file(const char* src, const char* dst) {
    FILE* sp = fopen(src, "r");
    if (sp == NULL) {
        log_error_fmt("failed to open the file for reading \"%s\"", src);
        abort();
    }

    FILE* dp = fopen(dst, "w");
    if (dp == NULL) {
        fclose(sp);
        log_error_fmt("failed to open the file for writing \"%s\"", src);
        abort();
    }

    int ch;
    while ((ch = fgetc(sp)) != EOF) {
        fputc(ch, dp);
    }

    fclose(sp);
    fclose(dp);
}
