#include <snprintf.h>

#if !defined(HAVE_SNPRINTF) || !defined(HAVE_VSNPRINTF)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if !defined(HAVE_VSNPRINTF)
int
vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    int retval;

#if defined(_MSC_VER)
    if (size != 0 && str != NULL) {
        retval = _vsnprintf(str, size, format, ap);

        /* guarantee null-termination even if truncated */
        str[size - 1] = '\0';
    }

    if (retval < 0) {
        FILE* nul = fopen("NUL", "wb");
        if (nul != NULL) {
            retval = vfprintf(nul, format, ap);
            fclose(nul);
        }
    }
#else
    FILE* fp;

    fp = tmpfile();
    if (fp == NULL) {
        return -1;
    }

    retval = vfprintf(fp, format, ap);

    if (retval >= 0 && size > 0 && str != NULL) {
        size_t to_read = (size_t)retval < (size - 1) ? (size_t)retval : (size - 1);
        rewind(fp);
        if (to_read > 0) {
            to_read = fread(str, 1, to_read, fp);
        }
        str[to_read] = '\0';
    }

    fclose(fp);
#endif

    return retval;
}
#endif

#if !defined(HAVE_SNPRINTF)
int
snprintf(char* str, size_t size, const char* format, ...) {
    va_list ap;
    int retval;

    va_start(ap, format);
    retval = vsnprintf(str, size, format, ap);
    va_end(ap);

    return retval;
}
#endif

#endif
