#pragma once

#if defined(_MSC_VER)

#if _MSC_VER >= 1900
#define HAVE_SNPRINTF
#endif

#elif defined(__linux__)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define HAVE_SNPRINTF
#elif defined(_BSD_SOURCE) || defined(_GNU_SOURCE) || defined(_DEFAULT_SOURCE)
#define HAVE_SNPRINTF
#endif

#endif

#if !defined(HAVE_SNPRINTF)

#include <stdio.h>
#include <stdarg.h>

int vsnprintf(char* str, size_t size, const char* format, va_list ap);
int snprintf(char* str, size_t size, const char* format, ...);

#endif
