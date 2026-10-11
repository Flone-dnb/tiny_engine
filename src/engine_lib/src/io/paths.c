#include <io/paths.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <io/log.h>
#include <misc/globals.h>

#if defined(WIN32)
#include <misc/wchar_funcs.h>
#define NOMINMAX
#include <Windows.h>
#include <KnownFolders.h>
#include <Shlobj.h>
#endif

static char cached_path_to_config_dir[2048] = {0};
static char cached_path_to_log_file[2048] = {0};

const char*
paths_get_config_dir(void) {
    if (cached_path_to_config_dir[0] == 0) {
#if defined(WIN32)
        wchar_t folder_wpath[MAX_PATH + 1] = {0};
        unsigned int path_len;
        size_t i;
        const HRESULT result = SHGetFolderPathW(
            NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, folder_wpath);
        if (result != S_OK) {
            log_error(__FILE__, __LINE__, "failed to query config dir path");
            abort();
        }

        char* path_tmp = wchar_to_char(folder_wpath, &path_len);

        /* copy path and replace slashes */
        if (path_len > 256) {
            log_error(__FILE__, __LINE__, "path to AppData folder is too long");
            abort();
        }
        for (i = 0; i < path_len; i++) {
            if (path_tmp[i] == '\\') {
                path_tmp[i] = '/';
            }
        }

        sprintf(
            cached_path_to_config_dir, "%s/tiny_engine/%s/config/", &path_tmp[0],
            globals_get_app_name());

        free(path_tmp);
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
        wchar_t folder_wpath[MAX_PATH + 1] = {0};
        unsigned int path_len;
        size_t i;
        const HRESULT result = SHGetFolderPathW(
            NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, folder_wpath);
        if (result != S_OK) {
            log_error(__FILE__, __LINE__, "failed to query config dir path");
            abort();
        }

        char* path_tmp = wchar_to_char(folder_wpath, &path_len);

        /* copy path and replace slashes */
        if (path_len > 256) {
            log_error(__FILE__, __LINE__, "path to AppData folder is too long");
            abort();
        }
        for (i = 0; i < path_len; i++) {
            if (path_tmp[i] == '\\') {
                path_tmp[i] = '/';
            }
        }

        sprintf(
            cached_path_to_log_file, "%s/tiny_engine/%s/log.txt", &path_tmp[0],
            globals_get_app_name());

        free(path_tmp);
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
