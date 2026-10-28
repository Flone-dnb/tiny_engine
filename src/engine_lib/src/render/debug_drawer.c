#include <render/debug_drawer.h>

#if defined(ENGINE_DEBUG_TOOLS)

#include <snprintf.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <render/font_manager.h>
#include <render/renderer.h>
#include <shape/aabb_shape.h>
#include <window.h>
#include <glad/gl.h>
#include <io/log.h>

#ifndef va_copy
#ifdef __va_copy
#define va_copy(dest, src) __va_copy((dest), (src))
#else
#define va_copy(dest, src) ((dest) = (src))
#endif
#endif

#define TE_DEBUG_DRAWER_AABB_INDEX_COUNT 24

/* fixed text height for drawing text, in range [0.0; 1.0] */
static float debug_drawer_default_text_height = 0.0275f;

typedef struct te_double_array {
    unsigned char* data1;
    unsigned char* data2;
    unsigned int item_sizeof;
    unsigned int size1;
    unsigned int size2;
    unsigned int capacity;
    unsigned int expand_size;
    bool curr_1;
} te_double_array;

static te_double_array*
double_array_create(unsigned int item_sizeof, unsigned int capacity) {
    te_double_array* array;

    if (capacity == 0) {
        log_error(__FILE__, __LINE__, "expected non zero capacity");
        abort();
    }

    array = malloc(sizeof(te_double_array));
    array->item_sizeof = item_sizeof;
    array->size1 = 0;
    array->size2 = 0;
    array->capacity = capacity;
    array->expand_size = capacity;

    array->data1 = malloc(item_sizeof * capacity);
    array->data2 = malloc(item_sizeof * capacity);

    array->curr_1 = true;

    return array;
}

static void
double_array_destroy(te_double_array* array) {
    free(array->data1);
    free(array->data2);

    free(array);
}

static unsigned int
double_array_get_size(te_double_array* array) {
    return array->curr_1 ? array->size1 : array->size2;
}

static void
double_array_add_item(te_double_array* array, void* item) {
    unsigned char* data1;
    unsigned char* data2;
    unsigned char* data;
    unsigned int* size;

    if (double_array_get_size(array) + 1 > array->capacity) {
        array->capacity += array->expand_size;
        data1 = malloc(array->item_sizeof * array->capacity);
        data2 = malloc(array->item_sizeof * array->capacity);
        memcpy(data1, array->data1, array->item_sizeof * array->size1);
        memcpy(data2, array->data2, array->item_sizeof * array->size2);
        free(array->data1);
        free(array->data2);
        array->data1 = data1;
        array->data2 = data2;
    }

    data = array->curr_1 ? array->data1 : array->data2;
    size = array->curr_1 ? &array->size1 : &array->size2;

    memcpy(data + (array->item_sizeof * (*size)), item, array->item_sizeof);
    (*size) += 1;
}

static void*
double_array_get_data(te_double_array* array) {
    return array->curr_1 ? array->data1 : array->data2;
}

static void
double_array_switch_to_empty(te_double_array* array) {
    array->curr_1 = !array->curr_1;
    if (array->curr_1) {
        array->size1 = 0;
    } else {
        array->size2 = 0;
    }
}

/* prepared data to render a glyph */
typedef struct te_debug_drawer_glyph {
    te_vec2 pos_offset;
    te_vec2 size;

    /* relative to font bitmap */
    te_vec2 glyph_rel_size;
    te_vec2 glyph_rel_offset;

    float distance_to_next_glyph;
} te_debug_drawer_glyph;

/* data needed to draw a text */
typedef struct te_debug_drawer_text {
    /* must be freed */
    char* text;

    /* array of prepared glyph data, size of this array is @ref text_len */
    te_debug_drawer_glyph* glyphs;

    /* strlen of @ref text */
    unsigned int text_len;

    /* if less than zero then text should be destroyed */
    float time_left_sec;

    te_vec4 color;

    /* -1 if should be automatically picked */
    te_vec2 pos;
} te_debug_drawer_text;

/* data needed to draw AABB */
typedef struct te_debug_drawer_aabb {
    te_aabb_shape aabb;

    te_vec3 color;

    /* if less than zero then item should be destroyed */
    float time_left_sec;
} te_debug_drawer_aabb;

/* data needed to draw a line */
typedef struct te_debug_drawer_line {
    te_vec3 from;
    te_vec3 to;

    /* if less than zero then item should be destroyed */
    float time_left_sec;
} te_debug_drawer_line;

typedef struct te_debug_drawer_triangle_solid {
    te_vec3 v0;
    te_vec3 v1;
    te_vec3 v2;

    te_vec4 rgba;

    /* if less than zero then item should be destroyed */
    float time_left_sec;
} te_debug_drawer_triangle_solid;

/* groups debug drawer data */
typedef struct te_debug_drawer {
    /* do not free this pointer */
    te_renderer* renderer;

    /* array of text to draw */
    te_double_array* texts;

    /* array of AABBs to draw */
    te_double_array* aabbs;

    /* array of lines to draw */
    te_double_array* lines;

    /* array of solid triangles to draw */
    te_double_array* triangles_solid;
} te_debug_drawer;

/* static to allow drawing easily from various places */
static te_debug_drawer drawer;

void
prv_debug_drawer_init(struct te_renderer* renderer) {
    drawer.texts = double_array_create(sizeof(te_debug_drawer_text), 16);
    drawer.aabbs = double_array_create(sizeof(te_debug_drawer_aabb), 32);
    drawer.lines = double_array_create(sizeof(te_debug_drawer_line), 16);
    drawer.triangles_solid = double_array_create(sizeof(te_debug_drawer_triangle_solid), 64);

    drawer.renderer = renderer;
}

void
prv_debug_drawer_free_text(te_debug_drawer_text* text) {
    free(text->text);
    free(text->glyphs);
}

void
prv_debug_drawer_deinit(void) {
    unsigned int i;

    /* free debug objects */
    te_debug_drawer_text* texts = double_array_get_data(drawer.texts);
    for (i = 0; i < double_array_get_size(drawer.texts); i++) {
        prv_debug_drawer_free_text(&texts[i]);
    }
    double_array_destroy(drawer.texts);
    drawer.texts = NULL;

    double_array_destroy(drawer.aabbs);
    drawer.aabbs = NULL;

    double_array_destroy(drawer.lines);
    drawer.lines = NULL;

    double_array_destroy(drawer.triangles_solid);
    drawer.triangles_solid = NULL;

    drawer.renderer = NULL;
}

void
debug_drawer_draw_aabb(te_aabb_shape* aabb, float time_sec, te_vec3 color) {
    if (drawer.renderer == NULL) {
        return;
    } else {
        te_debug_drawer_aabb new_item;
        new_item.aabb = *aabb;
        new_item.time_left_sec = time_sec;
        vec3_copy(color, new_item.color);

        double_array_add_item(drawer.aabbs, &new_item);
    }
}

void
debug_drawer_draw_line(te_vec3 from, te_vec3 to, float time_sec) {
    if (drawer.renderer == NULL) {
        return;
    } else {
        te_debug_drawer_line new_item;
        new_item.time_left_sec = time_sec;
        vec3_copy(from, new_item.from);
        vec3_copy(to, new_item.to);

        double_array_add_item(drawer.lines, &new_item);
    }
}

void
debug_drawer_draw_triangle_solid(
    te_vec3 v0, te_vec3 v1, te_vec3 v2, te_vec4 rgba, float time_sec) {
    if (drawer.renderer == NULL) {
        return;
    } else {
        te_debug_drawer_triangle_solid new_item;
        vec4_copy(rgba, new_item.rgba);
        vec3_copy(v0, new_item.v0);
        vec3_copy(v1, new_item.v1);
        vec3_copy(v2, new_item.v2);
        new_item.time_left_sec = time_sec;

        double_array_add_item(drawer.triangles_solid, &new_item);
    }
}

void
debug_drawer_draw_text(float time_sec, const char* text) {
    te_vec3 color;
    vec3_set(1.0f, 1.0f, 1.0f, color);
    debug_drawer_draw_text_color(text, time_sec, color);
}

void
debug_drawer_draw_text_fmt(float time_sec, const char* fmt, ...) {
    char* message;
    te_vec3 color;
    size_t size;
    int test_size;

    va_list args;
    va_list args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);

    test_size = vsnprintf(NULL, 0, fmt, args);
    if (test_size <= 0) {
        log_error(__FILE__, __LINE__, "failed to format last log message");
        abort();
    }
    size = (size_t)test_size;
    message = malloc(size + 1);
    memset(message, 0, size + 1);

    vsprintf(message, fmt, args_copy);

    va_end(args_copy);
    va_end(args);

    vec3_set(1.0f, 1.0f, 1.0f, color);
    debug_drawer_draw_text_color(message, time_sec, color);

    free(message);
}

void
debug_drawer_draw_text_color(const char* text, float time_sec, te_vec3 color) {
    te_vec2 pos;
    vec2_set(-1.0f, -1.0f, pos);
    debug_drawer_draw_text_color_pos(text, time_sec, color, pos);
}

void
debug_drawer_draw_text_color_pos(
    const char* text, float time_sec, te_vec3 color, te_vec2 pos) {
    if (drawer.renderer == NULL) {
        return;
    } else {
        te_font_manager* font_manager;
        size_t text_len;
        float font_scale;
        float font_height_rel;
        unsigned int i, window_width, window_height, font_bitmap_width, font_bitmap_height;

        /* init data */
        te_debug_drawer_text new_item;
        vec3_copy(color, new_item.color);
        new_item.color[3] = 1.0f;
        vec2_copy(pos, new_item.pos);
        new_item.time_left_sec = time_sec;

        /* copy text */
        text_len = strlen(text);
        new_item.text = malloc(sizeof(char) * (text_len + 1));
        memcpy(new_item.text, text, sizeof(char) * text_len);
        new_item.text[text_len] = 0;
        new_item.text_len = (unsigned int)text_len;

        font_manager = renderer_get_font_manager(drawer.renderer);
        window_get_size(renderer_get_window(drawer.renderer), &window_width, &window_height);
        font_height_rel =
            (float)font_manager_get_font_height_pix(font_manager) / (float)window_height;
        font_manager_get_font_bitmap_size(
            font_manager, &font_bitmap_width, &font_bitmap_height);

        /* cache glyphs */
        font_scale = debug_drawer_default_text_height / font_height_rel;
        new_item.glyphs = malloc(sizeof(te_debug_drawer_glyph) * new_item.text_len);
        for (i = 0; i < new_item.text_len; i++) {
            te_font_glyph* src =
                font_manager_get_glyph(font_manager, (unsigned int)new_item.text[i]);
            te_debug_drawer_glyph* dst = &new_item.glyphs[i];

            dst->distance_to_next_glyph =
                (float)(src->advance
                        >> 6) /* bitshift by 6 to get value in pixels (2^6 = 64) */
                * font_scale;

            if (src->width > 0) {
                vec2_set(
                    (float)src->bearing_x * font_scale, -(float)src->bearing_y * font_scale,
                    dst->pos_offset);
                vec2_set(
                    (float)src->width * font_scale, (float)src->height * font_scale,
                    dst->size);
                vec2_set(
                    (float)src->width / (float)font_bitmap_width,
                    (float)src->height / (float)font_bitmap_height, dst->glyph_rel_size);
                vec2_set(
                    (float)src->bitmap_x / (float)font_bitmap_width,
                    (float)src->bitmap_y / (float)font_bitmap_height, dst->glyph_rel_offset);
            } else {
                vec2_zero(dst->size);
            }
        }

        double_array_add_item(drawer.texts, &new_item);
    }
}

float
debug_drawer_get_default_text_height(void) {
    return debug_drawer_default_text_height;
}

void
prv_debug_drawer_draw(
    struct te_renderer* renderer, float delta_time_sec, te_mat4* view_mat, te_mat4* proj_mat) {
    unsigned int window_width;
    unsigned int window_height;
    unsigned int old_line_count;
    unsigned int old_text_count;
    unsigned int old_aabb_count;
    unsigned int old_triangle_count;
    unsigned int i;

    te_vec2 window_size;
    window_get_size(renderer_get_window(renderer), &window_width, &window_height);
    vec2_set((float)window_width, (float)window_height, window_size);

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* setup global matrices */
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((*proj_mat)[0]);

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf((*view_mat)[0]);

    glDisable(GL_TEXTURE_2D);

    old_triangle_count = double_array_get_size(drawer.triangles_solid);
    if (old_triangle_count > 0) {
        te_debug_drawer_triangle_solid* triangles;

        triangles = double_array_get_data(drawer.triangles_solid);
        double_array_switch_to_empty(drawer.triangles_solid);
        for (i = 0; i < old_triangle_count; i++) {
            te_debug_drawer_triangle_solid* triangle = &triangles[i];

            glColor4fv(triangle->rgba);

            glBegin(GL_TRIANGLES);
            glVertex3fv(triangle->v0);
            glVertex3fv(triangle->v1);
            glVertex3fv(triangle->v2);
            glEnd();

            /* update state */
            triangle->time_left_sec -= delta_time_sec;
            if (triangle->time_left_sec >= 0.0f) {
                double_array_add_item(drawer.triangles_solid, triangle);
            }
        }
    }

    old_line_count = double_array_get_size(drawer.lines);
    if (old_line_count > 0) {
        te_debug_drawer_line* lines;

        lines = double_array_get_data(drawer.lines);
        double_array_switch_to_empty(drawer.lines);
        for (i = 0; i < old_line_count; i++) {
            te_debug_drawer_line* line = &lines[i];

            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

            glBegin(GL_LINES);
            glVertex3fv(line->from);
            glVertex3fv(line->to);
            glEnd();

            /* update state */
            line->time_left_sec -= delta_time_sec;
            if (line->time_left_sec >= 0.0f) {
                double_array_add_item(drawer.lines, line);
            }
        }
    }

    old_aabb_count = double_array_get_size(drawer.aabbs);
    if (old_aabb_count > 0) {
        te_vec3 min, max;
        te_debug_drawer_aabb* aabbs;

        aabbs = double_array_get_data(drawer.aabbs);
        double_array_switch_to_empty(drawer.aabbs);
        for (i = 0; i < old_aabb_count; i++) {
            te_debug_drawer_aabb* data = &aabbs[i];

            vec3_sub(data->aabb.center, data->aabb.extents, min);
            vec3_add(data->aabb.center, data->aabb.extents, max);

            glColor4fv(data->color);

            glBegin(GL_LINES);
            /* bottom face */
            glVertex3f(min[0], min[1], min[2]);
            glVertex3f(max[0], min[1], min[2]);
            glVertex3f(max[0], min[1], min[2]);
            glVertex3f(max[0], max[1], min[2]);
            glVertex3f(max[0], max[1], min[2]);
            glVertex3f(min[0], max[1], min[2]);
            glVertex3f(min[0], max[1], min[2]);
            glVertex3f(min[0], min[1], min[2]);
            /* top face */
            glVertex3f(min[0], min[1], max[2]);
            glVertex3f(max[0], min[1], max[2]);
            glVertex3f(max[0], min[1], max[2]);
            glVertex3f(max[0], max[1], max[2]);
            glVertex3f(max[0], max[1], max[2]);
            glVertex3f(min[0], max[1], max[2]);
            glVertex3f(min[0], max[1], max[2]);
            glVertex3f(min[0], min[1], max[2]);
            /* vertical lines */
            glVertex3f(min[0], min[1], min[2]);
            glVertex3f(min[0], min[1], max[2]);
            glVertex3f(max[0], min[1], min[2]);
            glVertex3f(max[0], min[1], max[2]);
            glVertex3f(max[0], max[1], min[2]);
            glVertex3f(max[0], max[1], max[2]);
            glVertex3f(min[0], max[1], min[2]);
            glVertex3f(min[0], max[1], max[2]);
            glEnd();

            /* update state */
            data->time_left_sec -= delta_time_sec;
            if (data->time_left_sec >= 0.0f) {
                double_array_add_item(drawer.aabbs, data);
            }
        }
    }

    /* lastly draw text */
    old_text_count = double_array_get_size(drawer.texts);
    if (old_text_count > 0) {
        te_debug_drawer_text* texts;
        te_vec2 screen_pos;
        float text_height;
        float auto_screen_y;
        float font_height_rel, font_scale;
        unsigned int text_idx, font_bitmap_id;

        te_font_manager* font_manager = renderer_get_font_manager(drawer.renderer);
        font_height_rel =
            (float)font_manager_get_font_height_pix(font_manager) / (float)window_height;
        font_scale = debug_drawer_default_text_height / font_height_rel;

        glEnable(GL_TEXTURE_2D);
        font_bitmap_id = font_manager_get_font_bitmap_id(font_manager);
        glBindTexture(GL_TEXTURE_2D, font_bitmap_id);

        /* we want to specify text in pixels */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0f, (float)window_width, (float)window_height, 0.0f, -1.0f, 1.0f);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        /* prepare starting position for the first text (relative to screen's top-left corner)
         * x will be reset on every text so it's defined below */
#if defined(ENGINE_EDITOR)
        /* start putting text slightly lower due to FPS stats text in the editor */
        screen_pos[1] = (float)window_height * 0.2f;
#else
        screen_pos[1] = (float)window_height * 0.1f;
#endif

        texts = double_array_get_data(drawer.texts);
        double_array_switch_to_empty(drawer.texts);
        for (text_idx = 0; text_idx < old_text_count; text_idx++) {
            te_debug_drawer_text* text = &texts[text_idx];

            screen_pos[0] = window_size[0] * 0.025f;
            if (text->pos[0] >= 0.0f) {
                screen_pos[0] = window_size[0] * text->pos[0];
            }
            text_height = window_size[1] * font_height_rel * font_scale;

            /* switch to the first row of the text */
            auto_screen_y = screen_pos[1]; /* save to restore later */
            if (text->pos[1] >= 0.0f) {
                screen_pos[1] = window_size[1] * text->pos[1];
            }
            screen_pos[1] += text_height;

            glColor4fv(text->color);

            /* draw each glyph */
            for (i = 0; i < text->text_len; i++) {
                te_debug_drawer_glyph* glyph = &text->glyphs[i];
                if (glyph->size[0] == 0.0f) {
                    screen_pos[0] += glyph->distance_to_next_glyph;
                } else {
                    te_vec2 glyph_pos;
                    vec2_add(screen_pos, glyph->pos_offset, glyph_pos);

                    glBegin(GL_QUADS);

                    glTexCoord2fv(glyph->glyph_rel_offset);
                    glVertex2fv(glyph_pos);

                    glTexCoord2f(
                        glyph->glyph_rel_offset[0] + glyph->glyph_rel_size[0],
                        glyph->glyph_rel_offset[1]);
                    glVertex2f(glyph_pos[0] + glyph->size[0], glyph_pos[1]);

                    glTexCoord2f(
                        glyph->glyph_rel_offset[0] + glyph->glyph_rel_size[0],
                        glyph->glyph_rel_offset[1] + glyph->glyph_rel_size[1]);
                    glVertex2f(glyph_pos[0] + glyph->size[0], glyph_pos[1] + glyph->size[1]);

                    glTexCoord2f(
                        glyph->glyph_rel_offset[0],
                        glyph->glyph_rel_offset[1] + glyph->glyph_rel_size[1]);
                    glVertex2f(glyph_pos[0], glyph_pos[1] + glyph->size[1]);

                    glEnd();
                }
                screen_pos[0] += text->glyphs[i].distance_to_next_glyph;
            }

            /* restore Y */
            if (text->pos[1] > 0.0f) {
                screen_pos[1] = auto_screen_y;
            }

            /* update state */
            text->time_left_sec -= delta_time_sec;
            if (text->time_left_sec < 0.0f) {
                prv_debug_drawer_free_text(text);
            } else {
                double_array_add_item(drawer.texts, text);
            }
        }

        glDisable(GL_TEXTURE_2D);
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    glDisable(GL_BLEND);
}

#endif
