#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#if defined(WIN32)
#define NOMINMAX
#include <direct.h>
#include <windows.h>
#include <tchar.h>
#define mkdir(dir, mode) _mkdir(dir)
#elif __linux__
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#else
#error "unsupported OS"
#endif

#define bool unsigned char
#define true 1
#define false 0

/* height of the font (in pixels) to load */
/* you can increase this if you have big text and it looks blurry */
#define FONT_HEIGHT_PIX_TO_LOAD 90

wchar_t*
wchar_from_char(const char* src, unsigned int* dst_strlen) {
    size_t len;
    wchar_t* dst;

    len = mbstowcs(NULL, src, 0);
    if (len == (size_t)-1) {
        printf("failed to convert char* to wchar*\n");
        abort();
    }

    dst = (wchar_t*)malloc(sizeof(wchar_t) * (len + 1));
    mbstowcs(dst, src, len + 1);

    if (len > 0xffffffff) {
        printf("failed to convert wchar* to char* - text is too long\n");
        abort();
    }
    if (dst_strlen != NULL) {
        (*dst_strlen) = (unsigned int)len;
    }

    return dst;
}

bool
filesystem_does_path_exists(const char* path) {
#if defined(WIN32)
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        DWORD attrib = GetFileAttributes(path);
        return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
    } else {
        fclose(fp);
        return true;
    }
#elif __linux__
    struct stat buffer;
    return stat(path, &buffer) == 0;
#else
#error "unsupported OS"
#endif
}

int
main(void) {
    FILE* fp;
    stbtt_fontinfo font;
    size_t elem_count_read;
    unsigned int bitmap_byte_count;
    unsigned int bitmap_byte_idx, bitmap_bit_idx;
    unsigned char* bitmap_data;
    unsigned long file_size;
    unsigned int tmp_uint;
    wchar_t* wchars_to_load;
    unsigned int wchars_len;
    unsigned int i, j;
    char* chars_to_load;
    unsigned char* ttf_buffer;
    unsigned char* glyph_bitmap;
    float scale;
    int glyph_w, glyph_h;
    int x0, y0, x1, y1;
    int advance, lsb;
    int offset_x, offset_y;

    if (!filesystem_does_path_exists(PATH_TO_TTF)) {
        printf("path \"%s\" does not exist\n", PATH_TO_TTF);
        return 1;
    }

    if (!filesystem_does_path_exists(PATH_TO_CHARS_TO_LOAD)) {
        printf("expected to find \"%s\" next to the binary\n", PATH_TO_CHARS_TO_LOAD);
        return 1;
    }

    /* load ttf into memory (stb_truetype requires the buffer to stay alive) */
    fp = fopen(PATH_TO_TTF, "rb");
    if (fp == NULL) {
        printf("failed to open ttf file \"%s\"\n", PATH_TO_TTF);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    {
        long size = ftell(fp);
        if (size < 0) {
            printf("failed to get ttf file size\n");
            return 1;
        }
        file_size = (unsigned long)size;
    }
    rewind(fp);

    ttf_buffer = malloc(file_size);
    elem_count_read = fread(ttf_buffer, 1, file_size, fp);
    fclose(fp);
    if (elem_count_read != file_size) {
        printf("failed to read the ttf file\n");
        return 1;
    }

    if (!stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer, 0))) {
        printf("failed to init stb_truetype font\n");
        return 1;
    }

    scale = stbtt_ScaleForMappingEmToPixels(&font, (float)FONT_HEIGHT_PIX_TO_LOAD);

    fp = fopen(PATH_TO_CHARS_TO_LOAD, "rb");
    if (fp == NULL) {
        printf("failed to open file \"%s\"\n", PATH_TO_CHARS_TO_LOAD);
        return 1;
    }

    /* get file size */
    file_size = 0;
    fseek(fp, 0, SEEK_END);
    {
        long size = ftell(fp);
        if (size < 0) {
            printf("failed to get file size\n");
            return 1;
        }
        file_size = (unsigned long)size;
    }
    rewind(fp);

    /* read content */
    chars_to_load = malloc(sizeof(char) * (file_size + 1));
    elem_count_read = fread(chars_to_load, 1, file_size, fp);
    chars_to_load[file_size] = 0;
    if (elem_count_read != file_size) {
        printf("failed to read the \"%s\"\n", PATH_TO_CHARS_TO_LOAD);
        return 1;
    }
    fclose(fp);

    wchars_to_load = wchar_from_char(chars_to_load, &wchars_len);
    free(chars_to_load);

    /* don't read further than new line or end of line */
    for (i = 0; i < wchars_len; i++) {
        if (wchars_to_load[i] == '\n' || wchars_to_load[i] == '\r') {
            printf("stopping chars reading on the new line / end of line\n");
            wchars_len = i;
            break;
        }
    }
    if (wchars_len == 0) {
        printf("no characters to load\n");
        return 1;
    }
    printf("found %u chars to load\n", wchars_len);

    /* count how much bits we will need for the bitmap */
    bitmap_byte_count = 0;
    bitmap_bit_idx = 0;
    for (i = 0; i < wchars_len; i++) {
        unsigned int char_code = (unsigned int)wchars_to_load[i];

        stbtt_GetCodepointHMetrics(&font, (int)char_code, &advance, &lsb);
        stbtt_GetCodepointBitmapBox(&font, (int)char_code, scale, scale, &x0, &y0, &x1, &y1);

        glyph_w = x1 - x0;
        glyph_h = y1 - y0;

        /* don't skip if width is 0, we still need to know distance to next glyph for such cases */

        for (j = 0; j < (unsigned int)(glyph_w * glyph_h); j++) {
            bitmap_bit_idx += 1;
            if (bitmap_bit_idx == 8) {
                bitmap_bit_idx = 0;
                bitmap_byte_count += 1;
            }
        }
    }
    if (bitmap_bit_idx > 0) {
        bitmap_byte_count += 1;
    }

    bitmap_data = malloc(bitmap_byte_count);
    memset(bitmap_data, 0, bitmap_byte_count);

    fp = fopen(PATH_TO_BITMAP, "wb");
    if (fp == NULL) {
        printf("failed to open the output file for writing\n");
        return 1;
    }

    tmp_uint = FONT_HEIGHT_PIX_TO_LOAD;
    fwrite(&tmp_uint, sizeof(tmp_uint), 1, fp);
    fwrite(&wchars_len, sizeof(wchars_len), 1, fp);

    bitmap_bit_idx = 0;
    bitmap_byte_idx = 0;
    for (i = 0; i < wchars_len; i++) {
        unsigned int char_code = (unsigned int)wchars_to_load[i];

        stbtt_GetCodepointHMetrics(&font, (int)char_code, &advance, &lsb);
        stbtt_GetCodepointBitmapBox(&font, (int)char_code, scale, scale, &x0, &y0, &x1, &y1);

        glyph_w = x1 - x0;
        glyph_h = y1 - y0;

        /* bitmap left and top (stb gives y0 relative to baseline, FreeType's bitmap_top is positive up) */
        offset_x = x0;
        offset_y =
            -y0; /* stb y0 is negative above baseline, so -y0 gives top offset (positive up) */

        /* advance 1/64 like freetype */
        {
            int advance_26_6 = (int)((float)advance * scale * 64.0f + 0.01f);
            if (advance_26_6 < 0)
                advance_26_6 = 0;
            tmp_uint = (unsigned int)advance_26_6;
        }

        fwrite(&char_code, sizeof(char_code), 1, fp);
        fwrite(&glyph_w, sizeof(glyph_w), 1, fp);
        fwrite(&glyph_h, sizeof(glyph_h), 1, fp);
        fwrite(&offset_x, sizeof(offset_x), 1, fp);
        fwrite(&offset_y, sizeof(offset_y), 1, fp);
        fwrite(&tmp_uint, sizeof(tmp_uint), 1, fp);

        /* render glyph into a temporary buffer if non-empty */
        if (glyph_w > 0 && glyph_h > 0) {
            glyph_bitmap = malloc((unsigned long)(glyph_w * glyph_h));
            stbtt_MakeCodepointBitmap(
                &font, glyph_bitmap, glyph_w, glyph_h, glyph_w, scale, scale, (int)char_code);

            for (j = 0; j < (unsigned int)(glyph_w * glyph_h); j++) {
                bitmap_data[bitmap_byte_idx] |= (glyph_bitmap[j] & 1) << bitmap_bit_idx;
                bitmap_bit_idx += 1;
                if (bitmap_bit_idx == 8) {
                    bitmap_bit_idx = 0;
                    bitmap_byte_idx += 1;
                }
            }
            free(glyph_bitmap);
        }
    }

    fwrite(bitmap_data, bitmap_byte_count, 1, fp);

    fclose(fp);
    free(wchars_to_load);
    free(bitmap_data);
    free(ttf_buffer);

    printf("successfully generated font bitmap at \"%s\"\n", PATH_TO_BITMAP);

    return 0;
}
