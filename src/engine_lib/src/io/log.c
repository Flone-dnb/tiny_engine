#include "io/log.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "io/filesystem.h"
#include "io/paths.h"

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
    unsigned long filename_start = 0;
    for (unsigned long i = strlen(filepath) - 1; i > 0; i--) {
        if (filepath[i] == '/' || filepath[i] == '\\') {
            filename_start = i + 1;
            break;
        }
    }

    // Create log prefix.
    char log_prefix[512] = {0};
    snprintf(&log_prefix[0], 511, "[%s] [%s] [%s:%d]", time_str, category_str, filepath + filename_start, line);

    // Open log file.
    const char* path_to_log_file = paths_get_log_file();
    filesystem_ensure_dirs_exist(path_to_log_file);
    FILE* log_file = fopen(path_to_log_file, "a");

    fprintf(log_file, "%s %s\n", log_prefix, message);
#if defined(DEBUG)
    // Also print to the terminal in debug builds.
    printf("%s %s\n", log_prefix, message);
#endif

    fclose(log_file);
}

void
prv_log_fmt(enum te_log_category category, char* filepath, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    unsigned long size = (unsigned long)snprintf(NULL, 0, fmt, args);
    char* message = malloc(size + 1ul);
    memset(message, 0, size + 1ul);

    vsprintf(message, fmt, args);

    va_end(args);

    prv_log(category, message, filepath, line);

    free(message);
}
