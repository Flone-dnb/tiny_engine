#pragma once

#include "cglm/vec2.h"
#include "cglm/vec4.h"

typedef struct te_widget_renderer te_widget_renderer;
struct te_renderer;

// Prepared data to render a text glyph.
typedef struct te_text_widget_glyph {
    // Position offset (from the pivot) in pixels.
    vec2 offset_pix;
    vec2 size_pix;

    // 0 if " " (space) character
    unsigned int tex_id;
} te_text_widget_glyph;

// Data needed to render a text widget.
typedef struct te_text_widget_render_data {
    // Array of prepared glyph data, size of this array is @ref glyph_count.
    te_text_widget_glyph* glyphs;

    // RGBA color of the text.
    vec4 color;

    // Position of the text's pivot point in pixels.
    vec2 pos_pix;

    // Size of the array @ref glyphs.
    unsigned int glyph_count;
} te_text_widget_render_data;

// Data needed to render a rect widget.
typedef struct te_rect_widget_render_data {
    // Position and size in pixels.
    vec2 pos_pix;
    vec2 size_pix;

    // RGBA color.
    vec4 color;

    // XY stores clip start in range [0.0; 1.0] and ZW stores clip size in the same range.
    vec4 clip_rect;

    // 0 if not used
    unsigned int tex_id;
} te_rect_widget_render_data;

te_widget_renderer* widget_renderer_create(struct te_renderer* renderer);
void widget_renderer_destroy(te_widget_renderer* widget_renderer);

// Draws widgets on the currently set framebuffer.
void widget_renderer_draw(te_widget_renderer* widget_renderer);

unsigned int widget_renderer_add_text_widget(te_widget_renderer* renderer);
void widget_renderer_remove_text_widget(te_widget_renderer* renderer, unsigned int handle);

unsigned int widget_renderer_add_rect_widget(te_widget_renderer* renderer);
void widget_renderer_remove_rect_widget(te_widget_renderer* renderer, unsigned int handle);

// Never store/save pointer to render data because on the next frame
// the pointer may end up pointing to an invalid memory. Only use "get_render_data" function to quickly
// update some render data. Suffix "_tmp" is used because of this.
te_text_widget_render_data* widget_renderer_get_text_widget_render_data_tmp(te_widget_renderer* renderer, unsigned int handle);
te_rect_widget_render_data* widget_renderer_get_rect_widget_render_data_tmp(te_widget_renderer* renderer, unsigned int handle);
