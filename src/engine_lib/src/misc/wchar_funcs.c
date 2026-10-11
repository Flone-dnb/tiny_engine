#include <misc/wchar_funcs.h>

#include <stdlib.h>
#include <string.h>
#include <io/log.h>
#include <limits.h>

char*
wchar_to_char(const wchar_t* src, unsigned int* dst_strlen) {
    size_t len;
    char* dst;

    len = wcstombs(NULL, src, 0);
    if (len == (size_t)-1) {
        log_error(__FILE__, __LINE__, "failed to convert wchar* to char*");
        abort();
    }

    dst = malloc(sizeof(char) * (len + 1));
    wcstombs(dst, src, len + 1);

    if (len > 0xffffffff) {
        log_error(__FILE__, __LINE__, "failed to convert wchar* to char* - text is too long");
        abort();
    }
    if (dst_strlen != NULL) {
        (*dst_strlen) = (unsigned int)len;
    }

    return dst;
}

wchar_t*
wchar_from_char(const char* src, unsigned int* dst_strlen) {
    size_t len;
    wchar_t* dst;

    len = mbstowcs(NULL, src, 0);
    if (len == (size_t)-1) {
        log_error(__FILE__, __LINE__, "failed to convert char* to wchar*");
        abort();
    }

    dst = malloc(sizeof(wchar_t) * (len + 1));
    mbstowcs(dst, src, len + 1);

    if (len > 0xffffffff) {
        log_error(__FILE__, __LINE__, "failed to convert wchar* to char* - text is too long");
        abort();
    }
    if (dst_strlen != NULL) {
        (*dst_strlen) = (unsigned int)len;
    }

    return dst;
}
