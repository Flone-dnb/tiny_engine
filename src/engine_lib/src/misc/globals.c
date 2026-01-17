#include "misc/globals.h"

#include <string.h>
#include "misc/error.h"

#if defined(WIN32)
#define NOMINMAX
#include <windows.h>
#elif __linux__
#include <unistd.h>
#else
#error "unsupported OS"
#endif

static char max_app_name_len = 64;
static char cached_app_name[64] = {0};

const char*
globals_get_app_name(void) {
    if (cached_app_name[0] == 0) {
        char buffer[1024] = {0};

        // Get full path.
#if defined(WIN32)
        if (GetModuleFileNameA(NULL, buffer, 1024) == 1024) {
            show_error_and_abort("failed to get path to the application");
        }
#elif __linux__
        if (readlink("/proc/self/exe", &buffer[0], 1024) == -1) {
            show_error_and_abort("failed to get path to the application");
        }
#else
#error "unsupported OS"
#endif

        // Find last slash in the path.
        unsigned long path_len = strlen(buffer);
        unsigned long last_slash_pos = path_len;
        for (unsigned long i = path_len - 1; i > 0; i--) {
            if (buffer[i] == '/' || buffer[i] == '\\') {
                last_slash_pos = i;
                break;
            }
        }
        if (last_slash_pos == path_len) {
            show_error_and_abort("unable to extract application name from the application path");
        }

        // Save app name.
        for (unsigned long src = last_slash_pos + 1, dst = 0;
             src < path_len && dst < (unsigned long)max_app_name_len; src++, dst++) {
            cached_app_name[dst] = buffer[src];
        }
    }

    return &cached_app_name[0];
}

void
globals_get_world_forward(float out[4]) {
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = -1.0f;
    out[3] = 0.0f;
}

void
globals_get_world_right(float out[4]) {
    out[0] = 1.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;
    out[3] = 0.0f;
}

void
globals_get_world_up(float out[4]) {
    out[0] = 0.0f;
    out[1] = 1.0f;
    out[2] = 0.0f;
    out[3] = 0.0f;
}
