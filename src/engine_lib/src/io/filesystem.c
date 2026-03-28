#include <io/filesystem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io/log.h>

#if defined(WIN32)
#define NOMINMAX
#include <direct.h>
#include <windows.h>
#include <tchar.h>
#define mkdir(dir, mode) _mkdir(dir)
#elif __linux__
#include <dirent.h>
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

te_filesystem_entry*
filesystem_list_directory(const char* path_to_dir, unsigned int* entry_count) {
    (*entry_count) = 0;

#if defined(__linux__)
    // Count number of entries.
    DIR* dir = opendir(path_to_dir);
    if (dir == NULL) {
        log_error_fmt("unable to open the directory \"%s\" (does path exist?)", path_to_dir);
        abort();
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' || entry->d_name[1] == '.') {
            continue;
        }
        (*entry_count) += 1;
    }
    closedir(dir);

    if ((*entry_count) == 0) {
        return NULL;
    }
    te_filesystem_entry* entries = malloc(sizeof(te_filesystem_entry) * (*entry_count));

    // Save entries.
    dir = opendir(path_to_dir);
    if (dir == NULL) {
        log_error_fmt("unable to open the directory \"%s\" (does path exist?)", path_to_dir);
        abort();
    }
    unsigned int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' || entry->d_name[1] == '.') {
            continue;
        }

        const size_t len = strlen(entry->d_name);
        entries[i].name = malloc(sizeof(char) * (len + 1));
        memcpy(entries[i].name, entry->d_name, sizeof(char) * len);
        entries[i].name[len] = 0;

        entries[i].is_dir = entry->d_type == DT_DIR;

        i++;
    }
    closedir(dir);

    return entries;
#elif defined(WIN32)
    char abs_path[MAX_PATH * 2 + 2] = {0};
    _fullpath(abs_path, path_to_dir, MAX_PATH * 2);
    abs_path[strlen(abs_path)] = '*';

    // Count entries.
    {
        WIN32_FIND_DATA ffd;
        HANDLE hFind = FindFirstFileA(abs_path, &ffd);
        if (hFind == INVALID_HANDLE_VALUE) {
            log_error_fmt("unable to open the directory \"%s\" (does path exist?)", abs_path);
            abort();
        }
        (*entry_count) = 0;
        do {
            if (ffd.cFileName[0] == '.' || ffd.cFileName[1] == '.') {
                continue;
            }
            (*entry_count) += 1;
        } while (FindNextFile(hFind, &ffd) != 0);
        FindClose(hFind);
    }

    if ((*entry_count) == 0) {
        return NULL;
    }
    te_filesystem_entry* entries = malloc(sizeof(te_filesystem_entry) * (*entry_count));

    // Save entries.
    {
        WIN32_FIND_DATA ffd;
        HANDLE hFind = FindFirstFileA(abs_path, &ffd);
        if (hFind == INVALID_HANDLE_VALUE) {
            log_error_fmt("unable to open the directory \"%s\" (does path exist?)", abs_path);
            abort();
        }
        unsigned int i = 0;
        do {
            if (ffd.cFileName[0] == '.' || ffd.cFileName[1] == '.') {
                continue;
            }

            const size_t len = strlen(ffd.cFileName);
            entries[i].name = malloc(sizeof(char) * (len + 1));
            memcpy(entries[i].name, ffd.cFileName, sizeof(char) * len);
            entries[i].name[len] = 0;

            entries[i].is_dir = ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

            i += 1;
        } while (FindNextFile(hFind, &ffd) != 0);
        FindClose(hFind);
    }

    return entries;
#else
#error "unsupported OS"
#endif
}
