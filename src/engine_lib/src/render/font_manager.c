#include <render/font_manager.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <misc/num_hashtable.h>
#include <io/filesystem.h>
#include <io/log.h>
#include <io/paths.h>
#include <window.h>
#include <glad/gl.h>

struct te_font_manager {
    /* stores loaded glyphs */
    te_num_hashtable* cached_glyphs;

    /* OpenGL ID of the font bitmap texture */
    unsigned int font_bitmap_id;

    /* font height in pixels */
    unsigned int font_height_pix;

    /* in pixels */
    unsigned int font_bitmap_width;
    unsigned int font_bitmap_height;

    unsigned int unknown_glyph_code;
};

static void load_font_chars(
    te_font_manager* font_manager, FILE* fp, unsigned int char_count, unsigned long file_size);

te_font_manager*
prv_font_manager_create(void) {
    const char* font_bitmap_path = "res/engine/font_bitmap.bin";
    FILE* fp;
    unsigned long file_size;
    unsigned int char_count;

    te_font_manager* manager = malloc(sizeof(te_font_manager));
    manager->font_bitmap_id = 0;
    manager->font_height_pix = 0;
    manager->font_bitmap_width = 0;
    manager->font_bitmap_height = 0;
    manager->unknown_glyph_code = 0;

    /* load font bitmap */
    if (!filesystem_does_path_exists(font_bitmap_path)) {
        log_error_fmt(
            __FILE__, __LINE__, "expected the font file to exist at \"%s\"", font_bitmap_path);
        abort();
    }
    fp = fopen(font_bitmap_path, "rb");
    if (fp == NULL) {
        log_error_fmt(
            __FILE__, __LINE__, "failed to open the font file \"%s\"", font_bitmap_path);
        abort();
    }

    /* get file size */
    file_size = 0;
    fseek(fp, 0, SEEK_END);
    {
        long size = ftell(fp);
        if (size < 0) {
            log_error(__FILE__, __LINE__, "failed to get file size");
            abort();
        }
        file_size = (unsigned long)size;
    }
    rewind(fp);

    fread(&manager->font_height_pix, sizeof(manager->font_height_pix), 1, fp);

    fread(&char_count, sizeof(char_count), 1, fp);
    if (char_count == 0 || char_count > 1024 * 1024) {
        /* probably something wrong */
        log_error_fmt(
            __FILE__, __LINE__, "unexpected char count found in the font: %u", char_count);
        abort();
    }

    manager->cached_glyphs = num_hashtable_create(char_count, sizeof(te_font_glyph), NULL);

    load_font_chars(manager, fp, char_count, file_size);

    return manager;
}

void
prv_font_manager_destroy(te_font_manager* manager) {
    glDeleteTextures(1, &manager->font_bitmap_id);

    num_hashtable_destroy(manager->cached_glyphs);

    free(manager);
}

static void
load_font_chars(
    te_font_manager* font_manager, FILE* fp, unsigned int char_count,
    unsigned long file_size) {
    te_font_glyph* glyphs;
    te_font_glyph* glyph;
    unsigned char* bitmap;
    unsigned char* packed_bits;
    long curr_pos;
    unsigned int i, byte_idx, bit_idx, y, x;
    unsigned int gl_type;
    unsigned int gl_format;
    int prev_unpack_alignment;

    unsigned int glyph_padding = 4;             /* empty pixels around the glyph */
    unsigned int bitmap_size_x = 1024;          /* hardcoded width */
    unsigned int bitmap_size_y = glyph_padding; /* will be calculated below */
    unsigned int curr_x, max_row_height;

    glyphs = malloc(sizeof(te_font_glyph) * char_count);

    /* load glyph info and prepare final bitmap resolution */
    curr_x = glyph_padding;
    max_row_height = 0;
    for (i = 0; i < char_count; i++) {
        glyph = &glyphs[i];
        fread(&glyph->char_code, sizeof(glyph->char_code), 1, fp);
        fread(&glyph->width, sizeof(glyph->width), 1, fp);
        fread(&glyph->height, sizeof(glyph->height), 1, fp);
        fread(&glyph->bearing_x, sizeof(glyph->bearing_x), 1, fp);
        fread(&glyph->bearing_y, sizeof(glyph->bearing_y), 1, fp);
        fread(&glyph->advance, sizeof(glyph->advance), 1, fp);

        if (curr_x + glyph->width + glyph_padding > bitmap_size_x) {
            if (curr_x == 0) {
                log_error(
                    __FILE__, __LINE__,
                    "can't fit a single glyph in the predefined bitmap width");
                abort();
            }
            curr_x = glyph_padding;
            bitmap_size_y += max_row_height;
            max_row_height = 0;
        }

        glyph->bitmap_x = curr_x;
        glyph->bitmap_y = bitmap_size_y;

        num_hashtable_insert(font_manager->cached_glyphs, glyph->char_code, glyph);

        curr_x += glyph->width + glyph_padding;
        if (glyph->height + glyph_padding > max_row_height) {
            max_row_height = glyph->height + glyph_padding;
        }
    }
    bitmap_size_y += max_row_height;

    /* for now just take the first, later will take '?' char */
    font_manager->unknown_glyph_code = glyphs[0].char_code;

    font_manager->font_bitmap_width = bitmap_size_x;
    font_manager->font_bitmap_height = bitmap_size_y;

    bitmap = malloc(bitmap_size_x * bitmap_size_y);
    memset(bitmap, 0, bitmap_size_x * bitmap_size_y);

    /* read packed bitmap bits */
    curr_pos = ftell(fp);
    packed_bits = malloc(file_size - (unsigned long)curr_pos);
    fread(packed_bits, file_size - (unsigned long)curr_pos, 1, fp);

    /* load glyph bitmaps */
    byte_idx = 0;
    bit_idx = 0;
    for (i = 0; i < char_count; i++) {
        glyph = &glyphs[i];

        for (y = 0; y < glyph->height; y++) {
            for (x = 0; x < glyph->width; x++) {
                bitmap[bitmap_size_x * (glyph->bitmap_y + y) + (glyph->bitmap_x + x)] =
                    255 * ((packed_bits[byte_idx] >> bit_idx) & 1);
                bit_idx += 1;
                if (bit_idx == 8) {
                    bit_idx = 0;
                    byte_idx += 1;
                }
            }
        }
    }

    /* load bitmap to the GPU */
    gl_type = GL_UNSIGNED_BYTE;
    gl_format = GL_LUMINANCE;

    /* set byte-alignment to 1 because we will create single-channel textures */
    prev_unpack_alignment = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack_alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    {
        glGenTextures(1, &font_manager->font_bitmap_id);

        glBindTexture(GL_TEXTURE_2D, font_manager->font_bitmap_id);
        {
#if !defined(ENGINE_GLES)
            /* generating a mipmap as a lazy way to remove aliasing from the font */
            const unsigned int mip_to_use = 1;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mip_to_use);
            /* generates mipmaps on the next glTexImage2D call */
            glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
#endif
            /* GLES 2.0 will have crunchy font for now since GL_TEXTURE_MAX_LEVEL and GL_TEXTURE_BASE_LEVEL are not available there */

            glTexImage2D(
                GL_TEXTURE_2D, 0, (GLint)gl_format, (int)bitmap_size_x, (int)bitmap_size_y, 0,
                (GLenum)gl_format, gl_type, bitmap);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); /* don't use mipmaps here */
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

#if !defined(ENGINE_GLES)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, mip_to_use);
#endif
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack_alignment);

    free(bitmap);
    free(packed_bits);
    free(glyphs);
    fclose(fp);

    glyph = num_hashtable_find(font_manager->cached_glyphs, '?');
    if (glyph != NULL) {
        font_manager->unknown_glyph_code = '?';
    }
}

te_font_glyph*
font_manager_get_glyph(te_font_manager* manager, unsigned int char_code) {
    te_font_glyph* glyph = num_hashtable_find(manager->cached_glyphs, char_code);
    if (glyph == NULL) {
        glyph = num_hashtable_find(manager->cached_glyphs, manager->unknown_glyph_code);
        if (glyph == NULL) {
            log_error_fmt(
                __FILE__, __LINE__,
                "unable to find a glyph with char code %u in the font bitmap (also can't find "
                "unknown glyph code)",
                char_code);
            abort();
        }
    }

    return glyph;
}

unsigned int
font_manager_get_font_bitmap_id(te_font_manager* manager) {
    return manager->font_bitmap_id;
}

unsigned int
font_manager_get_font_height_pix(te_font_manager* manager) {
    return manager->font_height_pix;
}

void
font_manager_get_font_bitmap_size(
    te_font_manager* manager, unsigned int* width, unsigned int* height) {
    (*width) = manager->font_bitmap_width;
    (*height) = manager->font_bitmap_height;
}
