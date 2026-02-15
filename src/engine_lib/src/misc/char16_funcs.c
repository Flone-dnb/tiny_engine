#include "misc/char16_funcs.h"

#include <stdlib.h>
#include <string.h>
#include "limits.h"
#include "misc/error.h"

unsigned int
char16_strlen(const char16_t* text) {
    char16_t* str = (char16_t*)text;
    for (; *str; str++) {}
    return (unsigned int)(str - text);
}

char*
char16_to_char(const char16_t* src, unsigned int src_strlen, unsigned int* dst_strlen) {
    char temp[MB_LEN_MAX];
    mbstate_t state;

    char* dst = malloc(sizeof(char) * (src_strlen * MB_LEN_MAX + 1));
    size_t dst_len = 0;

    for (size_t i = 0; i < src_strlen; i++) {
        const size_t len = c16rtomb(temp, src[i], &state);

        memcpy(dst + dst_len, temp, len);
        dst_len += len;
    }

    // Shrink.
    dst = realloc(dst, sizeof(char) * (dst_len + 1));
    dst[dst_len] = 0;

    if (dst_len > 0xffffffff) {
        show_error_and_abort("text too long");
    }
    (*dst_strlen) = (unsigned int)dst_len;
    return dst;
}

char16_t*
char16_from_char(const char* src, unsigned int src_strlen, unsigned int* dst_strlen) {
    char16_t temp;
    mbstate_t state;

    char16_t* dst = malloc(sizeof(char16_t) * (src_strlen + 1));
    size_t dst_len = 0;

    size_t rc = (size_t)-1;
    const char* curr = src;
    const char* end = src + src_strlen;
    while (dst_len < src_strlen) {
        rc = mbrtoc16(&temp, curr, (size_t)(end - curr + 1), &state);
        if (rc == (size_t)-3 || rc == (size_t)-2) {
            continue;
        } else if (rc == (size_t)-1 || rc == 0) {
            break;
        } else {
            dst[dst_len] = temp;
            dst_len += 1;
            curr += rc;
        }
    };

    // Shrink.
    dst = realloc(dst, sizeof(char16_t) * (dst_len + 1));
    dst[dst_len] = 0;

    if (dst_len > 0xffffffff) {
        show_error_and_abort("text too long");
    }
    (*dst_strlen) = (unsigned int)dst_len;
    return dst;
}
