#include <render/debug_drawer.h>

#if defined(ENGINE_DEBUG_TOOLS)

#include <snprintf.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <render/font_manager.h>
#include <render/renderer.h>
#include <render/shader_manager.h>
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
    uint8_t* data1;
    uint8_t* data2;
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
    uint8_t* data1;
    uint8_t* data2;
    uint8_t* data;
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

    /* 0 if " " (space) character */
    unsigned int tex_id;

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

/* data for the text shader program */
typedef struct te_debug_drawer_text_shader {
    unsigned int prog_id;

    int uniform_in_pos;
    int uniform_in_size;
    int uniform_clip_rect;
    int uniform_window_size;
    int uniform_text_color;
} te_debug_drawer_text_shader;

/* data for the AABB shader program */
typedef struct te_debug_drawer_aabb_shader {
    unsigned int prog_id;

    int uniform_pos_offset;
    int uniform_extents;
    int uniform_color;
    int uniform_view_proj_mat;
} te_debug_drawer_aabb_shader;

/* data for the line shader program */
typedef struct te_debug_drawer_line_shader {
    unsigned int prog_id;

    int uniform_from;
    int uniform_to;
    int uniform_view_proj_mat;
} te_debug_drawer_line_shader;

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

    /* quad geometry */
#if !defined(ENGINE_GLES)
    unsigned int vao_quad;
#endif
    unsigned int vbo_quad;
    unsigned int ebo_quad;

    /* AABB geometry */
#if !defined(ENGINE_GLES)
    unsigned int vao_aabb;
#endif
    unsigned int vbo_aabb;
    unsigned int ebo_aabb;

    /* line geometry */
#if !defined(ENGINE_GLES)
    unsigned int vao_line;
#endif
    unsigned int vbo_line;
    unsigned int ebo_line;

    te_debug_drawer_text_shader text_shader;
    te_debug_drawer_aabb_shader aabb_shader;
    te_debug_drawer_line_shader line_shader;
} te_debug_drawer;

/* static to allow drawing easily from various places */
static te_debug_drawer drawer;

void
prv_debug_drawer_init(struct te_renderer* renderer) {
    drawer.texts = double_array_create(sizeof(te_debug_drawer_text), 16);
    drawer.aabbs = double_array_create(sizeof(te_debug_drawer_aabb), 32);
    drawer.lines = double_array_create(sizeof(te_debug_drawer_line), 16);

#if !defined(ENGINE_GLES)
    drawer.vao_quad = 0;
#endif
    drawer.vbo_quad = 0;
    drawer.ebo_quad = 0;

#if !defined(ENGINE_GLES)
    drawer.vao_aabb = 0;
#endif
    drawer.vbo_aabb = 0;
    drawer.ebo_aabb = 0;

#if !defined(ENGINE_GLES)
    drawer.vao_line = 0;
#endif
    drawer.vbo_line = 0;
    drawer.ebo_line = 0;

    drawer.renderer = renderer;

    /* load text shader */
    {
        te_shader_manager* shader_manager = renderer_get_shader_manager(renderer);
        drawer.text_shader.prog_id = shader_manager_request_shader(
            shader_manager, "engine/shader/quad.vert.glsl", "engine/shader/text.frag.glsl");

        drawer.text_shader.uniform_in_pos =
            get_uniform_location(drawer.text_shader.prog_id, "in_pos");
        drawer.text_shader.uniform_in_size =
            get_uniform_location(drawer.text_shader.prog_id, "in_size");
        drawer.text_shader.uniform_clip_rect =
            get_uniform_location(drawer.text_shader.prog_id, "clip_rect");
        drawer.text_shader.uniform_window_size =
            get_uniform_location(drawer.text_shader.prog_id, "window_size");
        drawer.text_shader.uniform_text_color =
            get_uniform_location(drawer.text_shader.prog_id, "text_color");
    }

    /* load AABB shader */
    {
        te_shader_manager* shader_manager = renderer_get_shader_manager(renderer);
        drawer.aabb_shader.prog_id = shader_manager_request_shader(
            shader_manager, "engine/shader/debug/aabb.vert.glsl",
            "engine/shader/debug/aabb.frag.glsl");

        drawer.aabb_shader.uniform_pos_offset =
            get_uniform_location(drawer.aabb_shader.prog_id, "pos_offset");
        drawer.aabb_shader.uniform_extents =
            get_uniform_location(drawer.aabb_shader.prog_id, "extents");
        drawer.aabb_shader.uniform_color =
            get_uniform_location(drawer.aabb_shader.prog_id, "color");
        drawer.aabb_shader.uniform_view_proj_mat =
            get_uniform_location(drawer.aabb_shader.prog_id, "view_proj_mat");
    }

    /* load line shader */
    {
        te_shader_manager* shader_manager = renderer_get_shader_manager(renderer);
        drawer.line_shader.prog_id = shader_manager_request_shader(
            shader_manager, "engine/shader/debug/line.vert.glsl",
            "engine/shader/debug/line.frag.glsl");

        drawer.line_shader.uniform_from =
            get_uniform_location(drawer.line_shader.prog_id, "from");
        drawer.line_shader.uniform_to = get_uniform_location(drawer.line_shader.prog_id, "to");
        drawer.line_shader.uniform_view_proj_mat =
            get_uniform_location(drawer.line_shader.prog_id, "view_proj_mat");
    }

    /* create quad geometry */
    {
        const unsigned short indices[6] = {0, 1, 2, 0, 2, 3};
        te_vec4 vertices[4]; /* XY pos, ZW uv */
        vec4_set(0.0f, 0.0f, 0.0f, 0.0f, &vertices[0][0]);
        vec4_set(0.0f, 1.0f, 0.0f, 1.0f, &vertices[1][0]);
        vec4_set(1.0f, 1.0f, 1.0f, 1.0f, &vertices[2][0]);
        vec4_set(1.0f, 0.0f, 1.0f, 0.0f, &vertices[3][0]);

#if !defined(ENGINE_GLES)
        glGenVertexArrays(1, &drawer.vao_quad);
#endif
        glGenBuffers(1, &drawer.vbo_quad);
        glGenBuffers(1, &drawer.ebo_quad);

#if !defined(ENGINE_GLES)
        glBindVertexArray(drawer.vao_quad);
#endif

        glBindBuffer(GL_ARRAY_BUFFER, drawer.vbo_quad);
        glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(te_vec4), &vertices[0][0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer.ebo_quad);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

#if defined(ENGINE_GLES)
        glBindAttribLocation(drawer.text_shader.prog_id, 0, "vertex");
#endif
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(te_vec4), NULL);

#if !defined(ENGINE_GLES)
        glBindVertexArray(0);
#endif
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    /* create AABB geometry (using lines) */
    {
        te_vec3 vertices[8];

        const unsigned short indices[TE_DEBUG_DRAWER_AABB_INDEX_COUNT] = {
            0, 1, 1, 2, 2, 3, 3, 0, // lower quad
            4, 5, 5, 6, 6, 7, 7, 4, // upper quad
            0, 4, 1, 5, 2, 6, 3, 7  // vertical lines
        };

        te_vec3 extents;
        vec3_set(1.0f, 1.0f, 1.0f, extents);

        vec3_set(-extents[0], -extents[1], -extents[2], &vertices[0][0]);
        vec3_set(+extents[0], -extents[1], -extents[2], &vertices[1][0]);
        vec3_set(+extents[0], -extents[1], +extents[2], &vertices[2][0]);
        vec3_set(-extents[0], -extents[1], +extents[2], &vertices[3][0]);

        vec3_set(-extents[0], +extents[1], -extents[2], &vertices[4][0]);
        vec3_set(+extents[0], +extents[1], -extents[2], &vertices[5][0]);
        vec3_set(+extents[0], +extents[1], +extents[2], &vertices[6][0]);
        vec3_set(-extents[0], +extents[1], +extents[2], &vertices[7][0]);

#if !defined(ENGINE_GLES)
        glGenVertexArrays(1, &drawer.vao_aabb);
#endif
        glGenBuffers(1, &drawer.vbo_aabb);
        glGenBuffers(1, &drawer.ebo_aabb);

#if !defined(ENGINE_GLES)
        glBindVertexArray(drawer.vao_aabb);
#endif

        glBindBuffer(GL_ARRAY_BUFFER, drawer.vbo_aabb);
        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(te_vec3), &vertices[0][0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer.ebo_aabb);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER, 24 * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

#if defined(ENGINE_GLES)
        glBindAttribLocation(drawer.aabb_shader.prog_id, 0, "local_pos");
#endif
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(te_vec3), NULL);

#if !defined(ENGINE_GLES)
        glBindVertexArray(0);
#endif
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    /* create line geometry */
    {
        const unsigned short indices[2] = {0, 1};
        te_vec3 vertices[2];

        vec3_set(0.0f, 0.0f, 0.0f, &vertices[0][0]);
        vec3_set(1.0f, 1.0f, 1.0f, &vertices[1][0]);

#if !defined(ENGINE_GLES)
        glGenVertexArrays(1, &drawer.vao_line);
#endif
        glGenBuffers(1, &drawer.vbo_line);
        glGenBuffers(1, &drawer.ebo_line);

#if !defined(ENGINE_GLES)
        glBindVertexArray(drawer.vao_line);
#endif

        glBindBuffer(GL_ARRAY_BUFFER, drawer.vbo_line);
        glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(te_vec3), &vertices[0][0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer.ebo_line);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER, 2 * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

#if defined(ENGINE_GLES)
        glBindAttribLocation(drawer.line_shader.prog_id, 0, "local_pos");
#endif
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(te_vec3), NULL);

#if !defined(ENGINE_GLES)
        glBindVertexArray(0);
#endif
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

void
prv_debug_drawer_free_text(te_debug_drawer_text* text) {
    free(text->text);
    free(text->glyphs);
}

void
prv_debug_drawer_deinit(struct te_renderer* renderer) {
    te_shader_manager* shader_manager;
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

    /* free shaders */
    shader_manager = renderer_get_shader_manager(renderer);
    shader_manager_mark_unused_shader(shader_manager, drawer.text_shader.prog_id);
    shader_manager_mark_unused_shader(shader_manager, drawer.aabb_shader.prog_id);
    shader_manager_mark_unused_shader(shader_manager, drawer.line_shader.prog_id);

    /* free geometry */
#if !defined(ENGINE_GLES)
    glDeleteVertexArrays(1, &drawer.vao_quad);
#endif
    glDeleteBuffers(1, &drawer.vbo_quad);
    glDeleteBuffers(1, &drawer.ebo_quad);

#if !defined(ENGINE_GLES)
    glDeleteVertexArrays(1, &drawer.vao_aabb);
#endif
    glDeleteBuffers(1, &drawer.vbo_aabb);
    glDeleteBuffers(1, &drawer.ebo_aabb);

#if !defined(ENGINE_GLES)
    glDeleteVertexArrays(1, &drawer.vao_line);
#endif
    glDeleteBuffers(1, &drawer.vbo_line);
    glDeleteBuffers(1, &drawer.ebo_line);

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
        unsigned int i;

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

        /* cache glyphs */
        font_manager = renderer_get_font_manager(drawer.renderer);
        font_scale =
            debug_drawer_default_text_height / prv_font_manager_get_font_height_to_load();
        new_item.glyphs = malloc(sizeof(te_debug_drawer_glyph) * new_item.text_len);
        for (i = 0; i < new_item.text_len; i++) {
            te_font_glyph src =
                font_manager_get_glyph(font_manager, (unsigned long)new_item.text[i]);
            te_debug_drawer_glyph* dst = &new_item.glyphs[i];

            dst->distance_to_next_glyph =
                (float)(src.advance >> 6) /* bitshift by 6 to get value in pixels (2^6 = 64) */
                * font_scale;

            if (src.width == 0) {
                dst->tex_id = 0;
            } else {
                dst->tex_id = src.tex_id;
                vec2_set(
                    (float)src.bearing_x * font_scale, -(float)src.bearing_y * font_scale,
                    dst->pos_offset);
                vec2_set(
                    (float)src.width * font_scale, (float)src.height * font_scale, dst->size);
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
    struct te_renderer* renderer, float delta_time_sec, te_mat4* view_proj_mat) {
    unsigned int window_width;
    unsigned int window_height;
    unsigned int old_line_count;
    unsigned int old_text_count;
    unsigned int old_aabb_count;
    unsigned int i;

    te_vec2 window_size;
    window_get_size(renderer_get_window(renderer), &window_width, &window_height);
    vec2_set((float)window_width, (float)window_height, window_size);

    glDisable(GL_DEPTH_TEST);

    old_line_count = double_array_get_size(drawer.lines);
    if (old_line_count > 0) {
        te_debug_drawer_line* lines;

        glUseProgram(drawer.line_shader.prog_id);

#if defined(ENGINE_GLES)
        glBindBuffer(GL_ARRAY_BUFFER, drawer.vbo_line);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer.ebo_line);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), NULL);
#else
        glBindVertexArray(drawer.vao_line);
#endif

        glUniformMatrix4fv(
            drawer.line_shader.uniform_view_proj_mat, 1, GL_FALSE, (*view_proj_mat)[0]);

        lines = double_array_get_data(drawer.lines);
        double_array_switch_to_empty(drawer.lines);
        for (i = 0; i < old_line_count; i++) {
            te_debug_drawer_line* line = &lines[i];

            glUniform3fv(drawer.line_shader.uniform_from, 1, line->from);
            glUniform3fv(drawer.line_shader.uniform_to, 1, line->to);

            glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, NULL);

            /* update state */
            line->time_left_sec -= delta_time_sec;
            if (line->time_left_sec >= 0.0f) {
                double_array_add_item(drawer.lines, line);
            }
        }
    }

    old_aabb_count = double_array_get_size(drawer.aabbs);
    if (old_aabb_count > 0) {
        te_debug_drawer_aabb* aabbs;

        glUseProgram(drawer.aabb_shader.prog_id);

#if defined(ENGINE_GLES)
        glBindBuffer(GL_ARRAY_BUFFER, drawer.vbo_aabb);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer.ebo_aabb);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), NULL);
#else
        glBindVertexArray(drawer.vao_aabb);
#endif

        glUniformMatrix4fv(
            drawer.aabb_shader.uniform_view_proj_mat, 1, GL_FALSE, (*view_proj_mat)[0]);

        aabbs = double_array_get_data(drawer.aabbs);
        double_array_switch_to_empty(drawer.aabbs);
        for (i = 0; i < old_aabb_count; i++) {
            te_debug_drawer_aabb* data = &aabbs[i];

            glUniform3fv(drawer.aabb_shader.uniform_pos_offset, 1, data->aabb.center);
            glUniform3fv(drawer.aabb_shader.uniform_extents, 1, data->aabb.extents);
            glUniform3fv(drawer.aabb_shader.uniform_color, 1, data->color);

            glDrawElements(
                GL_LINES, TE_DEBUG_DRAWER_AABB_INDEX_COUNT, GL_UNSIGNED_SHORT, NULL);

            /* update state */
            data->time_left_sec -= delta_time_sec;
            if (data->time_left_sec >= 0.0f) {
                double_array_add_item(drawer.aabbs, data);
            }
        }
    }

    old_text_count = double_array_get_size(drawer.texts);
    if (old_text_count > 0) {
        te_debug_drawer_text* texts;
        te_vec4 clip_rect;
        te_vec2 screen_pos;
        float text_height;
        float auto_screen_y;
        unsigned int text_idx;
        const float font_height = prv_font_manager_get_font_height_to_load();
        const float font_scale = debug_drawer_default_text_height / font_height;

        glUseProgram(drawer.text_shader.prog_id);

#if defined(ENGINE_GLES)
        glBindBuffer(GL_ARRAY_BUFFER, drawer.vbo_quad);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer.ebo_quad);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), NULL);
#else
        glBindVertexArray(drawer.vao_quad);
#endif

        glActiveTexture(GL_TEXTURE0); /* glyph's bitmap */

        vec4_set(0.0f, 0.0f, 1.0f, 1.0f, clip_rect);
        glUniform4fv(drawer.text_shader.uniform_clip_rect, 1, clip_rect);

        glUniform2fv(drawer.text_shader.uniform_window_size, 1, window_size);

        /* prepare starting position for the first text (relative to screen's top-left corner)
         * x will be reset on every text so it's defined below */
        screen_pos[1] = (float)window_height * 0.1f;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        texts = double_array_get_data(drawer.texts);
        double_array_switch_to_empty(drawer.texts);
        for (text_idx = 0; text_idx < old_text_count; text_idx++) {
            te_debug_drawer_text* text = &texts[text_idx];

            screen_pos[0] = window_size[0] * 0.025f;
            if (text->pos[0] >= 0.0f) {
                screen_pos[0] = window_size[0] * text->pos[0];
            }
            text_height = window_size[1] * font_height * font_scale;

            /* switch to the first row of the text */
            auto_screen_y = screen_pos[1]; /* save to restore later */
            if (text->pos[1] >= 0.0f) {
                screen_pos[1] = window_size[1] * text->pos[1];
            }
            screen_pos[1] += text_height;

            glUniform4fv(drawer.text_shader.uniform_text_color, 1, text->color);

            /* draw each glyph */
            for (i = 0; i < text->text_len; i++) {
                if (text->glyphs[i].tex_id == 0) {
                    screen_pos[0] += text->glyphs[i].distance_to_next_glyph;
                } else {
                    te_vec2 glyph_pos;
                    vec2_add(screen_pos, text->glyphs[i].pos_offset, glyph_pos);

                    glUniform2fv(drawer.text_shader.uniform_in_pos, 1, glyph_pos);
                    glUniform2fv(drawer.text_shader.uniform_in_size, 1, text->glyphs[i].size);

                    glBindTexture(GL_TEXTURE_2D, text->glyphs[i].tex_id);

                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
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

        glDisable(GL_BLEND);
    }

    glEnable(GL_DEPTH_TEST);
}

#endif
