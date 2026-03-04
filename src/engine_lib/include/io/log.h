#pragma once

#include <stdarg.h>

enum te_log_category { LOG_INFO, LOG_WARN, LOG_ERROR };

#define log_info(message) prv_log(LOG_INFO, message, __FILE__, __LINE__)
#define log_warn(message) prv_log(LOG_WARN, message, __FILE__, __LINE__)
#define log_error(message) prv_log(LOG_ERROR, message, __FILE__, __LINE__)

#define log_info_fmt(fmt, ...) prv_log_fmt(LOG_INFO, __FILE__, __LINE__, fmt, __VA_ARGS__);
#define log_warn_fmt(fmt, ...) prv_log_fmt(LOG_WARN, __FILE__, __LINE__, fmt, __VA_ARGS__);
#define log_error_fmt(fmt, ...) prv_log_fmt(LOG_ERROR, __FILE__, __LINE__, fmt, __VA_ARGS__);

// Returns the total number of warnings that were logged at this point.
unsigned int log_get_warning_count_logged(void);

// Returns the total number of errors that were logged at this point.
unsigned int log_get_error_count_logged(void);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

void prv_log(enum te_log_category category, const char* message, char* filepath, int line);
void
prv_log_fmt(enum te_log_category category, char* filepath, int line, const char* fmt, ...);
