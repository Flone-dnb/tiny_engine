#include <io/log.h>

#include <snprintf.h>
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
log_info(const char* filepath, int line, const char* message) {
    prv_log(TE_LOG_INFO, message, filepath, line);
}

void
log_warn(const char* filepath, int line, const char* message) {
    prv_log(TE_LOG_WARN, message, filepath, line);
}

void
log_error(const char* filepath, int line, const char* message) {
    prv_log(TE_LOG_ERROR, message, filepath, line);
}

void
prv_log(enum te_log_category category, const char* message, const char* filepath, int line) {
    static char log_prefix[512] = {0};
    char time_str[32] = {0};
    const char* category_str = NULL;
    const char* path_to_log_file;
    struct tm* tm_info;
    FILE* log_file;
    vec3 color_warn = {1.0f, 1.0f, 0.0f};
    vec3 color_error = {1.0f, 0.0f, 0.0f};
    time_t t;
    unsigned long filename_start = 0;
    unsigned long i;

    memset(log_prefix, 0, sizeof(log_prefix));

    t = time(NULL);
    tm_info = localtime(&t);
    strftime(time_str, 32, "%H:%M:%S", tm_info);

    switch (category) {
        case (TE_LOG_INFO): {
            category_str = "info";
            break;
        }
        case (TE_LOG_WARN): {
            warn_count_logged += 1;
            category_str = "warn";
            break;
        }
        case (TE_LOG_ERROR): {
            error_count_logged += 1;
            category_str = "error";
            break;
        }
    }

    for (i = strlen(filepath) - 1; i > 0; i--) {
        if (filepath[i] == '/' || filepath[i] == '\\') {
            filename_start = i + 1;
            break;
        }
    }

    snprintf(
        &log_prefix[0], 511, "[%s] [%s] [%s:%d]", time_str, category_str,
        filepath + filename_start, line);

    /** open log file (not checking if directories exist because we checked this at game start) */
    path_to_log_file = paths_get_log_file();
    log_file = fopen(path_to_log_file, "a");
    if (log_file == NULL) {
        printf("failed to open log file \"%s\"\n", path_to_log_file);
#if defined(WIN32)
        printf("does User name contains special characters?\n");
#endif
        log_file = fopen("log.txt", "a");
        if (log_file == NULL) {
            printf("can't even create a log.txt file, logging is disabled, good luck");
            return;
        }
        fprintf(
            log_file, "ERROR: failed to create log file at path \"%s\"\n", path_to_log_file);
#if defined(WIN32)
        fprintf(log_file, "does User name contains special characters?\n");
#endif
        if (error_count_logged == 0) {
            error_count_logged += 1;
        }
    }

    fprintf(log_file, "%s %s\n", log_prefix, message);
#if defined(DEBUG)
    printf("%s %s\n", log_prefix, message);
#endif

#if defined(ENGINE_DEBUG_TOOLS)
    if (category == TE_LOG_WARN) {
        debug_drawer_draw_text_color(message, 5.0f, color_warn);
    } else if (category == TE_LOG_ERROR) {
        debug_drawer_draw_text_color(message, 5.0f, color_error);
    }
#endif

    fclose(log_file);
}

void
log_info_fmt(const char* filepath, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    prv_log_fmt(TE_LOG_INFO, filepath, line, fmt, args);
    va_end(args);
}

void
log_warn_fmt(const char* filepath, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    prv_log_fmt(TE_LOG_WARN, filepath, line, fmt, args);
    va_end(args);
}

void
log_error_fmt(const char* filepath, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    prv_log_fmt(TE_LOG_ERROR, filepath, line, fmt, args);
    va_end(args);
}

void
prv_log_fmt(
    enum te_log_category category, const char* filepath, int line, const char* fmt, ...) {
    va_list args;
    va_list args_copy;
    int size;
    char* message;

    va_start(args, fmt);
    va_copy(args_copy, args);

    size = vsnprintf(NULL, 0, fmt, args);
    if (size <= 0) {
        log_error("failed to format last log message");
        abort();
    }
    message = malloc((unsigned long)size + 1);
    memset(message, 0, (unsigned long)size + 1);

    vsprintf(message, fmt, args_copy);

    va_end(args_copy);
    va_end(args);

    prv_log(category, message, filepath, line);

    free(message);
}
