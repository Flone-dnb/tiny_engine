#include <misc/wchar_funcs.h>

#include <stdlib.h>
#include <string.h>
#include <io/log.h>
#include <limits.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

char*
wchar_to_char(const wchar_t* src, unsigned int* dst_strlen) {
    int required_bytes;
    char* dst;

    if (src == NULL)
        return NULL;

    required_bytes = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
    if (required_bytes <= 0) {
        log_error(__FILE__, __LINE__, "failed to convert wchar* to char*");
        abort();
    }

    dst = malloc(sizeof(char) * required_bytes);
    if (dst == NULL)
        abort();

    if (WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, required_bytes, NULL, NULL) == 0) {
        free(dst);
        log_error(__FILE__, __LINE__, "failed to convert wchar* to char*");
        abort();
    }

    /* required_bytes includes the null terminator */
    if (dst_strlen != NULL) {
        *dst_strlen = (unsigned int)(required_bytes - 1);
    }

    return dst;
}

wchar_t*
wchar_from_char(const char* src, unsigned int* dst_strlen) {
    int required_chars;
    wchar_t* dst;

    if (src == NULL)
        return NULL;

    required_chars = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
    if (required_chars <= 0) {
        log_error(__FILE__, __LINE__, "failed to convert char* to wchar*");
        abort();
    }

    dst = malloc(sizeof(wchar_t) * required_chars);
    if (dst == NULL)
        abort();

    if (MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, required_chars) == 0) {
        free(dst);
        log_error(__FILE__, __LINE__, "failed to convert char* to wchar*");
        abort();
    }

    /* required_chars includes the null terminator */
    if (dst_strlen != NULL) {
        *dst_strlen = (unsigned int)(required_chars - 1);
    }

    return dst;
}

#elif defined(__linux__)
#include <iconv.h>

char*
wchar_to_char(const wchar_t* src, unsigned int* dst_strlen) {
    iconv_t cd;
    size_t in_bytes, out_bytes, initial_out_bytes, res;
    char* src_ptr;
    char* dst_ptr;
    char* dst;

    if (src == NULL)
        return NULL;

    cd = iconv_open("UTF-8", "WCHAR_T");
    if (cd == (iconv_t)-1) {
        log_error(__FILE__, __LINE__, "iconv_open failed");
        abort();
    }

    in_bytes = wcslen(src) * sizeof(wchar_t);
    initial_out_bytes = (wcslen(src) * 4) + 1;

    dst = (char*)malloc(initial_out_bytes);
    if (dst == NULL)
        abort();

    src_ptr = (char*)src;
    dst_ptr = dst;
    out_bytes = initial_out_bytes;

    res = iconv(cd, &src_ptr, &in_bytes, &dst_ptr, &out_bytes);
    iconv_close(cd);

    if (res == (size_t)-1) {
        free(dst);
        log_error(__FILE__, __LINE__, "failed to convert wchar* to char*");
        abort();
    }

    *dst_ptr = '\0';

    if (dst_strlen != NULL) {
        *dst_strlen = (unsigned int)(dst_ptr - dst);
    }

    return dst;
}

wchar_t*
wchar_from_char(const char* src, unsigned int* dst_strlen) {
    iconv_t cd;
    size_t in_bytes, out_bytes, initial_out_bytes, res;
    char* src_ptr;
    char* dst_ptr;
    wchar_t* dst;

    if (src == NULL)
        return NULL;

    cd = iconv_open("WCHAR_T", "UTF-8");
    if (cd == (iconv_t)-1) {
        log_error(__FILE__, __LINE__, "iconv_open failed");
        abort();
    }

    in_bytes = strlen(src);
    initial_out_bytes = (in_bytes + 1) * sizeof(wchar_t);

    dst = malloc(initial_out_bytes);
    if (dst == NULL)
        abort();

    src_ptr = (char*)src;
    dst_ptr = (char*)dst;
    out_bytes = initial_out_bytes;

    res = iconv(cd, &src_ptr, &in_bytes, &dst_ptr, &out_bytes);
    iconv_close(cd);

    if (res == (size_t)-1) {
        free(dst);
        log_error(__FILE__, __LINE__, "failed to convert char* to wchar*");
        abort();
    }

    *((wchar_t*)dst_ptr) = L'\0';

    if (dst_strlen != NULL) {
        *dst_strlen = (unsigned int)(((wchar_t*)dst_ptr) - dst);
    }

    return dst;
}

#else
#error "unsupported OS"
#endif
