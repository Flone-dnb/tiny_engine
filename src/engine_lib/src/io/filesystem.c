#include <io/filesystem.h>

#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#endif

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
    const char pathSeparator = '/'; /* we convert \\ to / on windows */

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

bool
filesystem_path_is_directory(const char* path) {
#if defined(WIN32)
    DWORD dwAttrib = GetFileAttributesA(path);
    if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
        log_error_fmt(__FILE__, __LINE__, "failed to get path attributes for %s", path);
        return false;
    }
    if (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) {
        return true;
    }

    return false;
#elif __linux__
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        log_error_fmt(__FILE__, __LINE__, "failed to get path attributes for %s", path);
        return false;
    }

    if (S_ISDIR(path_stat.st_mode)) {
        return true;
    } else if (S_ISREG(path_stat.st_mode)) {
        return false;
    }

    return false;
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
        log_error_fmt(
            __FILE__, __LINE__, "failed to rename file from \"%s\" to \"%s\"", old_path,
            new_path);
        abort();
    }
}

void
filesystem_copy_file(const char* src, const char* dst) {
#if defined(WIN32)
    CopyFile(src, dst, 0);
#else
    struct stat file_stat = {0};
    int input, output, result;
    off_t copied;

    if ((input = open(src, O_RDONLY)) == -1) {
        return;
    }
    if ((output = creat(dst, 0660)) == -1) {
        close(input);
        return;
    }

    result = fstat(input, &file_stat);
    copied = 0;
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
    size_t i;
    size_t idx;
    const size_t len = strlen(path);

    idx = len;
    for (i = len - 1; i > 0; i--) {
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
        if (ret_len != NULL) {
            (*ret_len) = 0;
        }
        return NULL;
    }

    if (include_extension) {
        if (ret_len != NULL) {
            (*ret_len) = (unsigned int)(len - idx);
        }
        return path + idx;
    }

    for (i = idx; i < len; i++) {
        if (path[i] == '.') {
            if (ret_len != NULL) {
                (*ret_len) = (unsigned int)(i - idx);
            }
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
    char* dst;
    size_t i;
    size_t len;
    size_t start_pos = 0xFFFFFFFF;

    /* find `res/` in the path */
    len = strlen(src);
    start_pos = 0xFFFFFFFF;

#if defined(__linux__)
    if (strncmp(src, "res/", 4) == 0)
#else
    if (strncmp(src, "res\\", 4) == 0 || strncmp(src, "res/", 4) == 0)
#endif
    {
        start_pos = 4;
    } else {
        for (i = 1; i < len; i++) {
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

    dst = malloc(sizeof(char) * (len - start_pos + 1));
    memcpy(dst, src + start_pos, sizeof(char) * (len - start_pos));
    dst[len - start_pos] = 0;

    return dst;
}

char*
filesystem_get_parent_path(const char* path, unsigned int path_len, unsigned int* ret_strlen) {
    char* out;
    unsigned int pos;

    if (path_len == 0) {
        path_len = (unsigned int)strlen(path);
    }
    if (path_len <= 1) {
        return NULL;
    }

    pos = path_len - 1;
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

    out = malloc(sizeof(char) * (pos + 1));
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
    char* out;
    unsigned int out_len;
    bool have_slash;

    if (path_len == 0) {
        path_len = (unsigned int)strlen(path);
    }
    if (add_len == 0) {
        add_len = (unsigned int)strlen(add);
    }

    have_slash = path[path_len - 1] == '/' || path[path_len - 1] == '\\';

    out_len = path_len + !have_slash + add_len;
    out = malloc(sizeof(char) * (out_len + 1));

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
    char* out;
    unsigned int out_len;
    bool have_slash;

    if (path_len == 0) {
        path_len = (unsigned int)strlen(path);
    }
    if (add_len == 0) {
        add_len = (unsigned int)strlen(add);
    }
    if (extension_len == 0) {
        extension_len = (unsigned int)strlen(extension);
    }

    have_slash = path[path_len - 1] == '/' || path[path_len - 1] == '\\';

    out_len = path_len + !have_slash + add_len + extension_len;
    out = malloc(sizeof(char) * (out_len + 1));

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
#if defined(__linux__)
    DIR* dir;
    struct dirent* entry;
    te_filesystem_entry* entries;
    size_t len;
    unsigned int i;
#elif defined(WIN32)
    char abs_path[MAX_PATH * 2 + 2] = {0};
    te_filesystem_entry* entries;
    size_t len;
    WIN32_FIND_DATA ffd;
    HANDLE hFind;
    unsigned int i;
#endif

    (*entry_count) = 0;

#if defined(__linux__)
    /* count number of entries */
    dir = opendir(path_to_dir);
    if (dir == NULL) {
        log_error_fmt(
            __FILE__, __LINE__, "unable to open the directory \"%s\" (does path exist?)",
            path_to_dir);
        abort();
    }
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
    entries = malloc(sizeof(te_filesystem_entry) * (*entry_count));

    /* save entries */
    dir = opendir(path_to_dir);
    if (dir == NULL) {
        log_error_fmt(
            __FILE__, __LINE__, "unable to open the directory \"%s\" (does path exist?)",
            path_to_dir);
        abort();
    }
    i = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        len = strlen(entry->d_name);
        entries[i].name = malloc(sizeof(char) * (len + 1));
        memcpy(entries[i].name, entry->d_name, sizeof(char) * len);
        entries[i].name[len] = 0;

        entries[i].name_len = (unsigned int)len;

        entries[i].is_dir = entry->d_type == DT_DIR;

        i++;
    }
    closedir(dir);

    return entries;
#elif defined(WIN32)
    _fullpath(abs_path, path_to_dir, MAX_PATH * 2);

    /* prepare path for FindFirstFile */
    {
        len = strlen(abs_path);
        if (abs_path[len - 1] == '\\' || abs_path[len - 1] == '/') {
            abs_path[len] = '*';
        } else {
            abs_path[len] = '\\';
            abs_path[len + 1] = '*';
        }
    }

    /* count entries */
    {
        hFind = FindFirstFileA(abs_path, &ffd);
        if (hFind == INVALID_HANDLE_VALUE) {
            log_error_fmt(
                __FILE__, __LINE__, "unable to open the directory \"%s\" (does path exist?)",
                abs_path);
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
    entries = malloc(sizeof(te_filesystem_entry) * (*entry_count));

    /* save entries */
    {
        hFind = FindFirstFileA(abs_path, &ffd);
        if (hFind == INVALID_HANDLE_VALUE) {
            log_error_fmt(
                __FILE__, __LINE__, "unable to open the directory \"%s\" (does path exist?)",
                abs_path);
            abort();
        }
        i = 0;
        do {
            if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) {
                continue;
            }

            len = strlen(ffd.cFileName);
            entries[i].name = malloc(sizeof(char) * (len + 1));
            memcpy(entries[i].name, ffd.cFileName, sizeof(char) * len);
            entries[i].name[len] = 0;

            entries[i].name_len = (unsigned int)len;

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
