#pragma once

#if defined(_MSC_VER)

#if _MSC_VER >= 1500
#define HAVE_VSNPRINTF
#endif

#if _MSC_VER >= 1900
#define HAVE_VSNPRINTF
#define HAVE_SNPRINTF
#endif

#elif defined(__linux__)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define HAVE_VSNPRINTF
#define HAVE_SNPRINTF
#elif defined(_BSD_SOURCE) || defined(_GNU_SOURCE) || defined(_DEFAULT_SOURCE)
#define HAVE_VSNPRINTF
#define HAVE_SNPRINTF
#endif

#endif

#if !defined(HAVE_SNPRINTF) || !defined(HAVE_VSNPRINTF)

#include <stdio.h>
#include <stdarg.h>

#if !defined(HAVE_VSNPRINTF)
int vsnprintf(char* str, size_t size, const char* format, va_list ap);
#endif

#if !defined(HAVE_SNPRINTF)
int snprintf(char* str, size_t size, const char* format, ...);
#endif

#endif
