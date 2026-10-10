#include <snprintf.h>

#if !defined(HAVE_SNPRINTF)

#include <stdlib.h>
#include <string.h>

int
vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    int retval;

    if (str == NULL || size == 0) {
        return -1;
    }

#if defined(_MSC_VER) && (_MSC_VER < 1900)
    /* old msvc does not guarantee null terminated when overflow */
    retval = _vsnprintf(str, size, format, ap);
    if (retval < 0 || (size_t)retval >= size) {
        str[size - 1] = '\0';
        retval = -1;
    }
#else
    {
        size_t alloc_size = size + 512;
        char* tmp_buf = (char*)malloc(alloc_size);

        if (tmp_buf == NULL) {
            return -1;
        }

        retval = vsprintf(tmp_buf, format, ap);

        if (retval >= 0) {
            size_t copy_len = (size_t)retval;
            if (copy_len >= size) {
                copy_len = size - 1;
            }
            memcpy(str, tmp_buf, copy_len);
            str[copy_len] = '\0';
        }

        free(tmp_buf);
    }
#endif

    return retval;
}

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
