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
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
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

void
filesystem_create_directory(const char* path) {
    mkdir(path, 0755);
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
#if defined(WIN32)
    CopyFile(src, dst, 0);
#else
    int input, output;
    if ((input = open(src, O_RDONLY)) == -1) {
        return;
    }
    if ((output = creat(dst, 0660)) == -1) {
        close(input);
        return;
    }

    struct stat file_stat = {0};
    int result = fstat(input, &file_stat);
    off_t copied = 0;
    while (result == 0 && copied < file_stat.st_size) {
        ssize_t written = sendfile(output, input, &copied, SSIZE_MAX);
        copied += written;
        if (written == -1) {
            result = -1;
        }
    }

    close(input);
    close(output);
#endif
}

const char*
filesystem_find_filename(const char* path, bool include_extension, unsigned int* ret_len) {
    const size_t len = strlen(path);

    size_t idx = len;
    for (size_t i = len - 1; i > 0; i--) {
#if defined(WIN32)
        if (path[i] == '/' || path[i] == '\\') {
#else
        if (path[i] == '/') {
#endif
            idx = i + 1;
            break;
        }
    }
    if (idx >= len) {
        (*ret_len) = 0;
        return NULL;
    }

    if (include_extension) {
        (*ret_len) = (unsigned int)(len - idx);
        return path + idx;
    }

    for (size_t i = idx; i < len; i++) {
        if (path[i] == '.') {
            (*ret_len) = (unsigned int)(i - idx);
            break;
        }
    }

    return path + idx;
}

char*
filesystem_convert_path_to_absolute(const char* src) {
#if defined(__linux__)
    return realpath(src, NULL);
#elif defined(WIN32)
    return _fullpath(NULL, src, 0);
#else
#error "unsupported OS"
#endif
}

char*
filesystem_convert_path_to_relative(const char* src) {
    // Find `res/` in the path.
    const size_t len = strlen(src);
    size_t start_pos = 0xFFFFFFFF;

#if defined(__linux__)
    if (strncmp(src, "res/", 4) == 0)
#else
    if (strncmp(src, "res\\", 4) == 0 || strncmp(src, "res/", 4) == 0)
#endif
    {
        start_pos = 4;
    } else {
        for (size_t i = 1; i < len; i++) {
#if defined(__linux__)
            if (strncmp(src + i, "/res/", 5) == 0)
#else
            if (strncmp(src + i, "\\res\\", 5) == 0 || strncmp(src + i, "/res/", 5) == 0)
#endif
            {
                start_pos = i + 5;
                break;
            }
        }
    }
    if (start_pos == 0xFFFFFFFF) {
        return NULL;
    }

    char* dst = malloc(sizeof(char) * (len - start_pos + 1));
    memcpy(dst, src + start_pos, sizeof(char) * (len - start_pos));
    dst[len - start_pos] = 0;

    return dst;
}

char*
filesystem_get_parent_path(const char* path, unsigned int path_len, unsigned int* ret_strlen) {
    if (path_len == 0) {
        path_len = (unsigned int)strlen(path);
    }
    if (path_len <= 1) {
        return NULL;
    }

    unsigned int pos = path_len - 1;
#if defined(WIN32)
    if (path[pos] == '\\' || path[pos] == '/')
#else
    if (path[pos] == '/')
#endif
    {
        pos -= 1;
    }

    for (; pos > 0; pos--) {
#if defined(WIN32)
        if (path[pos] == '\\' || path[pos] == '/')
#else
        if (path[pos] == '/')
#endif
        {
            break;
        }
    }
    if (pos == 0) {
        return NULL;
    }

    char* out = malloc(sizeof(char) * (pos + 1));
    memcpy(out, path, sizeof(char) * pos);
    out[pos] = 0;

    if (ret_strlen != NULL) {
        (*ret_strlen) = pos;
    }

    return out;
}

char*
filesystem_prepend_res_to_path(const char* relative_path, unsigned int* ret_strlen) {
    unsigned int len = (unsigned int)strlen(relative_path);

    char* new_path = malloc(sizeof(char) * (len + 4 + 1));

#if defined(WIN32)
    memcpy(new_path, "res\\", sizeof(char) * 4);
#else
    memcpy(new_path, "res/", sizeof(char) * 4);
#endif
    memcpy(new_path + 4, relative_path, sizeof(char) * len);

    len += 4;
    new_path[len] = 0;

    if (ret_strlen != NULL) {
        (*ret_strlen) = len;
    }

    return new_path;
}

char*
filesystem_append_path(
    const char* path, unsigned int path_len, const char* add, unsigned int add_len,
    unsigned int* ret_strlen) {
    const bool have_slash = path[path_len - 1] == '/' || path[path_len - 1] == '\\';
    if (path_len == 0) {
        path_len = (unsigned int)strlen(path);
    }
    if (add_len == 0) {
        add_len = (unsigned int)strlen(add);
    }

    const unsigned int out_len = path_len + !have_slash + add_len;
    char* out = malloc(sizeof(char) * (out_len + 1));

    memcpy(out, path, sizeof(char) * path_len);
#if defined(WIN32)
    memcpy(out + path_len, "\\", sizeof(char) * !have_slash);
#else
    memcpy(out + path_len, "/", sizeof(char) * !have_slash);
#endif
    memcpy(out + path_len + !have_slash, add, sizeof(char) * add_len);

    out[out_len] = 0;

    if (ret_strlen != NULL) {
        (*ret_strlen) = out_len;
    }

    return out;
}

char*
filesystem_append_path_ext(
    const char* path, unsigned int path_len, const char* add, unsigned int add_len,
    const char* extension, unsigned int extension_len, unsigned int* ret_strlen) {
    const bool have_slash = path[path_len - 1] == '/' || path[path_len - 1] == '\\';
    if (path_len == 0) {
        path_len = (unsigned int)strlen(path);
    }
    if (add_len == 0) {
        add_len = (unsigned int)strlen(add);
    }
    if (extension_len == 0) {
        extension_len = (unsigned int)strlen(extension);
    }

    const unsigned int out_len = path_len + !have_slash + add_len + extension_len;
    char* out = malloc(sizeof(char) * (out_len + 1));

    memcpy(out, path, sizeof(char) * path_len);
#if defined(WIN32)
    memcpy(out + path_len, "\\", sizeof(char) * !have_slash);
#else
    memcpy(out + path_len, "/", sizeof(char) * !have_slash);
#endif
    memcpy(out + path_len + !have_slash, add, sizeof(char) * add_len);
    memcpy(out + path_len + !have_slash + add_len, extension, sizeof(char) * extension_len);

    out[out_len] = 0;

    if (ret_strlen != NULL) {
        (*ret_strlen) = out_len;
    }

    return out;
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
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
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
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
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

    // Prepare path for FindFirstFile.
    {
        const size_t len = strlen(abs_path);
        if (abs_path[len - 1] == '\\' || abs_path[len - 1] == '/') {
            abs_path[len] = '*';
        } else {
            abs_path[len] = '\\';
            abs_path[len + 1] = '*';
        }
    }

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
            if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) {
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
            if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) {
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
