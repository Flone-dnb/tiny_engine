#include <io/log.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <io/paths.h>
#include <render/debug_drawer.h>

static unsigned int error_count_logged = 0;
static unsigned int warn_count_logged = 0;

unsigned int
log_get_warning_count_logged(void) {
    return warn_count_logged;
}

unsigned int
log_get_error_count_logged(void) {
    return error_count_logged;
}

void
prv_log(enum te_log_category category, const char* message, char* filepath, int line) {
    // Prepare time string.
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    char time_str[32] = {0};
    strftime(time_str, 32, "%H:%M:%S", tm_info);

    // Prepare category string.
    const char* category_str = "info";
    if (category == LOG_WARN) {
        warn_count_logged += 1;
        category_str = "warn";
    } else if (category == LOG_ERROR) {
        error_count_logged += 1;
        category_str = "error";
    }

    // Find filename start in the filepath.
    size_t filename_start = 0;
    for (size_t i = strlen(filepath) - 1; i > 0; i--) {
        if (filepath[i] == '/' || filepath[i] == '\\') {
            filename_start = i + 1;
            break;
        }
    }

    // Create log prefix.
    char log_prefix[512] = {0};
    snprintf(&log_prefix[0], 511, "[%s] [%s] [%s:%d]", time_str, category_str, filepath + filename_start, line);

    // Open log file (not checking if directories exist because we checked this at game start).
    const char* path_to_log_file = paths_get_log_file();
    FILE* log_file = fopen(path_to_log_file, "a");
    if (log_file == NULL) {
        printf("failed to open log file \"%s\"\n", path_to_log_file);
#if defined(WIN32)
        printf("does User name contains special characters?\n");
#endif
        log_file = fopen("log.txt", "a");
        if (log_file == NULL) {
            abort();
        }
        fprintf(log_file, "ERROR: failed to create log file at path \"%s\"\n", path_to_log_file);
#if defined(WIN32)
        fprintf(log_file, "does User name contains special characters?\n");
#endif
        if (error_count_logged == 0) {
            error_count_logged += 1;
        }
    }

    fprintf(log_file, "%s %s\n", log_prefix, message);
#if defined(DEBUG)
    // Also print to the terminal in debug builds.
    printf("%s %s\n", log_prefix, message);
#endif

#if !defined(ENGINE_EDITOR) && defined(ENGINE_DEBUG_TOOLS)
    if (category == LOG_WARN) {
        debug_drawer_draw_text_color(message, 5.0f, (vec3){1.0f, 1.0f, 0.0f});
    } else if (category == LOG_ERROR) {
        debug_drawer_draw_text_color(message, 5.0f, (vec3){1.0f, 0.0f, 0.0f});
    }
#endif

    fclose(log_file);
}

void
prv_log_fmt(enum te_log_category category, char* filepath, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);

    int test_size = vsnprintf(NULL, 0, fmt, args);
    if (test_size <= 0) {
        log_error("failed to format last log message");
        abort();
    }
    size_t size = (size_t)test_size;
    char* message = malloc(size + 1);
    memset(message, 0, size + 1);

    vsprintf(message, fmt, args_copy);

    va_end(args_copy);
    va_end(args);

    prv_log(category, message, filepath, line);

    free(message);
}
