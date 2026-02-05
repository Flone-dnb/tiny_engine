#pragma once

// Handles font/glyth loading.
typedef struct te_font_manager te_font_manager;

// Font's glyph info.
typedef struct te_font_glyph {
    // Width and height in pixels.
    unsigned int width;
    unsigned int height;

    // Offset from baseline to the top-left corner of the glyph.
    int bearing_x;
    int bearing_y;

    // Character code of this glyph.
    unsigned long char_code;

    // Horizontal offset (in 1/64 pixels) until the next glyph.
    unsigned int advance;

    // OpenGL ID of the glyph texture.
    unsigned int tex_id;
} te_font_glyph;

struct te_renderer;

// Loads a font file (.ttf) specified relative to the `res` directory.
// Unloads old font if there was a font loaded previously.
void font_manager_load_font(te_font_manager* manager, const char* relative_path);

// Returns information about a cached glyph (loads and saves in the cache if was not cached previously).
// Also see @ref font_manager_cache_glyphs.
//
// @ref font_manager_load_font must be called previously.
te_font_glyph font_manager_get_glyph(te_font_manager* manager, unsigned long char_code);

// Makes sure the specified characters are in the cache (loads if needed).
// The specified character code range is inclusive.
void font_manager_cache_glyphs(te_font_manager* manager, unsigned long char_code_first,
                               unsigned long char_code_last);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Renderer is expected to create this manager.
te_font_manager* prv_font_manager_create(struct te_renderer* renderer);
void prv_font_manager_destroy(te_font_manager* manager);

// Called by the renderer after window size changed.
void prv_font_manager_on_window_size_changed(te_font_manager* manager);

// Returns font height in range [0.0; 1.0] that is used to load the font.
float prv_font_manager_get_font_height_to_load();
