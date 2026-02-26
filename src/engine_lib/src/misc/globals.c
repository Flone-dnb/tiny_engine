#include "misc/globals.h"

#include <stdlib.h>
#include <string.h>
#include "io/log.h"

#if defined(WIN32)
#define NOMINMAX
#include <windows.h>
#elif __linux__
#include <unistd.h>
#else
#error "unsupported OS"
#endif

static unsigned int max_app_name_len = 64;
static char cached_app_name[64] = {0};

const char*
globals_get_app_name(void) {
    if (cached_app_name[0] == 0) {
        char buffer[1024] = {0};

        // Get full path.
#if defined(WIN32)
        if (GetModuleFileNameA(NULL, buffer, 1024) == 1024) {
            log_error("failed to get path to the application");
            abort();
        }
#elif __linux__
        if (readlink("/proc/self/exe", &buffer[0], 1024) == -1) {
            log_error("failed to get path to the application");
            abort();
        }
#else
#error "unsupported OS"
#endif

        // Find last slash in the path.
        const size_t path_len = strlen(buffer);
        size_t last_slash_pos = path_len;
        for (size_t i = path_len - 1; i > 0; i--) {
            if (buffer[i] == '/' || buffer[i] == '\\') {
                last_slash_pos = i;
                break;
            }
        }
        if (last_slash_pos == path_len) {
            log_error("unable to extract application name from the application path");
            abort();
        }

        // Save app name.
        for (size_t src = last_slash_pos + 1, dst = 0; src < path_len && dst < max_app_name_len; src++, dst++) {
#if defined(WIN32)
            if (buffer[src] == '.') {
                // don't copy ".exe"
                break;
            }
#endif
            cached_app_name[dst] = buffer[src];
        }
    }

    return &cached_app_name[0];
}

void
globals_get_world_forward(float out[3]) {
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = -1.0f;
}

void
globals_get_world_right(float out[3]) {
    out[0] = 1.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;
}

void
globals_get_world_up(float out[3]) {
    out[0] = 0.0f;
    out[1] = 1.0f;
    out[2] = 0.0f;
}
