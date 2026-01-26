#include "io/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "misc/error.h"
#include "misc/globals.h"

static char cached_path_to_config_dir[2048] = {0};
static char cached_path_to_log_file[2048] = {0};

const char*
paths_get_config_dir(void) {
    if (cached_path_to_config_dir[0] == 0) {
#if defined(WIN32)
        PWSTR path_tmp = NULL;
        const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path_tmp);
        if (result != S_OK) {
            CoTaskMemFree(path_tmp);
            show_error_and_abort("failed to query config dir path");
        }

        // Copy path and replace slashes.
        char path_buff[256] = {0};
        const unsigned long path_len = strlen(path_tmp);
        if (path_len > 256) {
            show_error_and_abort("path to AppData folder is too long");
        }
        for (unsigned long i = 0; i < path_len; i++) {
            if (path_tmp[i] == '\\') {
                path_buff[i] = '/';
            } else {
                path_buff[i] = path_tmp[i];
            }
        }
        CoTaskMemFree(pPathTmp);

        sprintf(cached_path_to_config_dir, "%s/tiny_engine/%s/config/", &path_buff[0],
                globals_get_app_name());
#elif __linux__

#if defined(__aarch64__)
        // On ARM64 linux devices I've decided to store configs near the binary so that it will be easier to find them.
        sprintf(cached_path_to_config_dir, "config/");
#else
        char* home_path = getenv("HOME");
        if (home_path == NULL) {
            show_error_and_abort("unable to query environment variable HOME");
        }
        sprintf(cached_path_to_config_dir, "%s/.config/tiny_engine/%s/config/", home_path,
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
        PWSTR path_tmp = NULL;
        const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path_tmp);
        if (result != S_OK) {
            CoTaskMemFree(path_tmp);
            show_error_and_abort("failed to query config dir path");
        }

        // Copy path and replace slashes.
        char path_buff[256] = {0};
        const unsigned long path_len = strlen(path_tmp);
        if (path_len > 256) {
            show_error_and_abort("path to AppData folder is too long");
        }
        for (unsigned long i = 0; i < path_len; i++) {
            if (path_tmp[i] == '\\') {
                path_buff[i] = '/';
            } else {
                path_buff[i] = path_tmp[i];
            }
        }
        CoTaskMemFree(pPathTmp);

        sprintf(cached_path_to_log_file, "%s/tiny_engine/%s/log.txt", &path_buff[0], globals_get_app_name());
#elif __linux__

#if defined(__aarch64__)
        // On ARM64 linux devices I've decided to store logs near the binary so that it will be easier to find them.
        sprintf(cached_path_to_log_file, "log.txt");
#else
        char* home_path = getenv("HOME");
        if (home_path == NULL) {
            show_error_and_abort("unable to query environment variable HOME");
        }
        sprintf(cached_path_to_log_file, "%s/.config/tiny_engine/%s/log.txt", home_path,
                globals_get_app_name());
#endif

#else
#error "unsupported OS"
#endif
    }

    return &cached_path_to_log_file[0];
}

char*
paths_prepend_res_to_path(const char* relative_path) {
    const unsigned long len = strlen(relative_path);

    char* new_path = malloc(sizeof(char) * (len + 4 + 1));

    memcpy(new_path, "res/", sizeof(char) * 4);
    memcpy(new_path + 4, relative_path, sizeof(char) * len);

    new_path[len + 4] = 0;

    return new_path;
}
