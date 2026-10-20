#pragma once

/* handles font/glyth loading */
typedef struct te_font_manager te_font_manager;

/* font's glyph info */
typedef struct te_font_glyph {
    /* width and height in pixels, may be 0 for some glyphs (for example for space " " character) */
    unsigned int width;
    unsigned int height;

    /* offset from baseline to the top-left corner of the glyph */
    int bearing_x;
    int bearing_y;

    /* character code of this glyph */
    unsigned int char_code;

    /* horizontal offset (in 1/64 pixels) until the next glyph */
    unsigned int advance;

    /* starting X and Y position (in pixels) of this glyph on the font bitmap */
    unsigned int bitmap_x;
    unsigned int bitmap_y;
} te_font_glyph;

/* returns information about a cached glyph (loads and saves in the cache if was not cached previously)
 * also see @ref font_manager_cache_glyphs
 * 
 * @ref font_manager_load_font must be called previously */
te_font_glyph* font_manager_get_glyph(te_font_manager* manager, unsigned int char_code);

/* returns OpenGL ID of the font bitmap texture */
unsigned int font_manager_get_font_bitmap_id(te_font_manager* manager);

/* returns height of the font in pixels */
unsigned int font_manager_get_font_height_pix(te_font_manager* manager);

/* returns size of the font bitmap in pixels */
void font_manager_get_font_bitmap_size(
    te_font_manager* manager, unsigned int* width, unsigned int* height);

/** ------------------------------------------------------------------------------------------------
 *                                       PRIVATE API
 * ------------------------------------------------------------------------------------------------- */

/* renderer is expected to create this manager */
te_font_manager* prv_font_manager_create(void);
void prv_font_manager_destroy(te_font_manager* manager);
