#pragma once

#include <stdarg.h>

enum te_log_category { TE_LOG_INFO, TE_LOG_WARN, TE_LOG_ERROR };

/** pass __FILE__ and __LINE__ as first args */
void log_info(const char* filepath, int line, const char* message);
void log_warn(const char* filepath, int line, const char* message);
void log_error(const char* filepath, int line, const char* message);
void log_info_fmt(const char* filepath, int line, const char* fmt, ...);
void log_warn_fmt(const char* filepath, int line, const char* fmt, ...);
void log_error_fmt(const char* filepath, int line, const char* fmt, ...);

/** returns the total number of warnings that were logged at this point */
unsigned int log_get_warning_count_logged(void);

/** returns the total number of errors that were logged at this point */
unsigned int log_get_error_count_logged(void);

/** ------------------------------------------------------------------------------------------------
 *                                       PRIVATE API
 * ------------------------------------------------------------------------------------------------- */

void
prv_log(enum te_log_category category, const char* message, const char* filepath, int line);
void prv_log_fmt(
    enum te_log_category category, const char* filepath, int line, const char* fmt, va_list args);
