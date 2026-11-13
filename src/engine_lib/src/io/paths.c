#include <io/paths.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <io/log.h>
#include <misc/globals.h>

#if defined(WIN32)
#define NOMINMAX
#include <Windows.h>
#include <Shlobj.h>
#ifndef CSIDL_LOCAL_APPDATA
#define CSIDL_LOCAL_APPDATA 0x001C
#endif
#endif

static char cached_path_to_config_dir[2048] = {0};
static char cached_path_to_log_file[2048] = {0};

const char*
paths_get_config_dir(void) {
    if (cached_path_to_config_dir[0] == 0) {
#if defined(WIN32)
        char folder_path[MAX_PATH + 2] = {0};
        unsigned int path_len;
        size_t i;

        if (!SHGetSpecialFolderPathA(NULL, folder_path, CSIDL_LOCAL_APPDATA, FALSE)) {
            log_error(__FILE__, __LINE__, "failed to query config dir path");
            abort();
        }

        path_len = (unsigned int)strlen(folder_path);
        if (path_len == 0) {
            log_error(__FILE__, __LINE__, "failed to query config dir path");
            abort();
        }

        /* copy path and replace slashes */
        for (i = 0; i < path_len; i++) {
            if (folder_path[i] == '\\') {
                folder_path[i] = '/';
            }
        }
        if (folder_path[path_len - 1] == '/') {
            /* we expect no trailing slash */
            folder_path[path_len - 1] = 0;
        }

        sprintf(
            cached_path_to_config_dir, "%s/tiny_engine/%s/config/", folder_path,
            globals_get_app_name());
#elif __linux__

#if defined(__aarch64__)
        /* on ARM64 linux devices I've decided to store configs near the binary so that it will be easier to find them */
        sprintf(cached_path_to_config_dir, "config/");
#else
        char* home_path = getenv("HOME");
        if (home_path == NULL) {
            log_error(__FILE__, __LINE__, "unable to query environment variable HOME");
            abort();
        }
        sprintf(
            cached_path_to_config_dir, "%s/.config/tiny_engine/%s/config/", home_path,
            globals_get_app_name());
#endif

#else
#error "unsupported OS"
#endif
    }

    return &cached_path_to_config_dir[0];
}

const char*
paths_get_log_file(void) {
    if (cached_path_to_log_file[0] == 0) {
#if defined(WIN32)
        char folder_path[MAX_PATH + 2] = {0};
        unsigned int path_len;
        size_t i;

        if (!SHGetSpecialFolderPathA(NULL, folder_path, CSIDL_LOCAL_APPDATA, FALSE)) {
            log_error(__FILE__, __LINE__, "failed to query config dir path");
            abort();
        }

        path_len = (unsigned int)strlen(folder_path);
        if (path_len == 0) {
            log_error(__FILE__, __LINE__, "failed to query config dir path");
            abort();
        }

        /* copy path and replace slashes */
        for (i = 0; i < path_len; i++) {
            if (folder_path[i] == '\\') {
                folder_path[i] = '/';
            }
        }
        if (folder_path[path_len - 1] == '/') {
            /* we expect no trailing slash */
            folder_path[path_len - 1] = 0;
        }

        sprintf(
            cached_path_to_log_file, "%s/tiny_engine/%s/log.txt", folder_path,
            globals_get_app_name());
#elif __linux__

#if defined(__aarch64__)
        /* on ARM64 linux devices I've decided to store logs near the binary so that it will be easier to find them */
        sprintf(cached_path_to_log_file, "log.txt");
#else
        char* home_path = getenv("HOME");
        if (home_path == NULL) {
            log_error(__FILE__, __LINE__, "unable to query environment variable HOME");
            abort();
        }
        sprintf(
            cached_path_to_log_file, "%s/.config/tiny_engine/%s/log.txt", home_path,
            globals_get_app_name());
#endif

#else
#error "unsupported OS"
#endif
    }

    return &cached_path_to_log_file[0];
}
