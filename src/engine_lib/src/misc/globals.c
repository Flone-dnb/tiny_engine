#include <misc/globals.h>

#if defined(__linux__)
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <io/log.h>

#if defined(WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#else
#error "unsupported OS"
#endif

static unsigned int max_app_name_len = 64;
static char cached_app_name[64] = {0};

const char*
globals_get_app_name(void) {
    size_t i;
    size_t j;
    size_t path_len;
    size_t last_slash_pos;

    if (cached_app_name[0] == 0) {
        char buffer[1024] = {0};

        /** get full path */
#if defined(WIN32)
        if (GetModuleFileNameA(NULL, buffer, 1024) == 1024) {
            log_error(__FILE__, __LINE__, "failed to get path to the application");
            abort();
        }
#elif __linux__
        if (readlink("/proc/self/exe", &buffer[0], 1024) == -1) {
            log_error(__FILE__, __LINE__, "failed to get path to the application");
            abort();
        }
#else
#error "unsupported OS"
#endif

        /** find last slash in the path */
        path_len = strlen(buffer);
        last_slash_pos = path_len;
        for (i = path_len - 1; i > 0; i--) {
            if (buffer[i] == '/' || buffer[i] == '\\') {
                last_slash_pos = i;
                break;
            }
        }
        if (last_slash_pos == path_len) {
            log_error(
                __FILE__, __LINE__,
                "unable to extract application name from the application path");
            abort();
        }

        /** save app name */
        for (i = last_slash_pos + 1, j = 0; i < path_len && j < max_app_name_len; i++, j++) {
#if defined(WIN32)
            if (buffer[src] == '.') {
                /** don't copy ".exe" */
                break;
            }
#endif
            cached_app_name[j] = buffer[i];
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

float
globals_convert_string_to_float(const char* text, char** end) {
    char* curr = (char*)text;
    float out = 0.0f;
    float div = 1;
    bool after_dot = false;

    if (text == NULL) {
        (*end) = (char*)text;
        return 0.0f;
    }

    if (*curr == 0) {
        (*end) = curr;
        return out;
    }

    if ((*curr) == '-') {
        /** will negate later */
        curr++;
    }

    while (*curr != 0) {
        if ((*curr) >= '0' && (*curr) <= '9') {
            if (!after_dot) {
                out *= 10.0f;
                out += (float)((*curr) - '0');
            } else {
                div *= 10;
                out += (float)((*curr) - '0') / div;
            }
        } else if ((*curr) == '.' || (*curr) == ',') {
            after_dot = true;
        } else {
            break;
        }

        curr++;
    }

    if ((*text) == '-') {
        out *= -1.0f;
    }

    (*end) = curr - 1;
    return out;
}
