#include <render/debug_drawer.h>

#if defined(ENGINE_DEBUG_TOOLS)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <render/font_manager.h>
#include <render/renderer.h>
#include <render/shader_manager.h>
#include <shape/aabb_shape.h>
#include <window.h>
#include <glad/glad.h>
#include <io/log.h>

#define TE_DEBUG_DRAWER_AABB_INDEX_COUNT 24

// Fixed text height for drawing text, in range [0.0; 1.0].
static float debug_drawer_default_text_height = 0.0275f;

// Prepared data to render a glyph.
typedef struct te_debug_drawer_glyph {
    vec2 pos_offset;
    vec2 size;

    // 0 if " " (space) character
    unsigned int tex_id;

    float distance_to_next_glyph;
} te_debug_drawer_glyph;

// Data needed to draw a text.
typedef struct te_debug_drawer_text {
    // Must be freed.
    char* text;

    // Array of prepared glyph data, size of this array is @ref text_len.
    te_debug_drawer_glyph* glyphs;

    // strlen of @ref text.
    unsigned int text_len;

    // If less than zero then text should be destroyed.
    float time_left_sec;

    vec4 color;

    // -1 if should be automatically picked.
    vec2 pos;
} te_debug_drawer_text;

// Data needed to draw AABB.
typedef struct te_debug_drawer_aabb {
    te_aabb_shape aabb;

    // If less than zero then item should be destroyed.
    float time_left_sec;
} te_debug_drawer_aabb;

// Data needed to draw a line.
typedef struct te_debug_drawer_line {
    vec3 from;
    vec3 to;

    // If less than zero then item should be destroyed.
    float time_left_sec;
} te_debug_drawer_line;

// Data for the text shader program.
typedef struct te_debug_drawer_text_shader {
    unsigned int prog_id;

    int uniform_in_pos;
    int uniform_in_size;
    int uniform_clip_rect;
    int uniform_window_size;
    int uniform_text_color;
} te_debug_drawer_text_shader;

// Data for the AABB shader program.
typedef struct te_debug_drawer_aabb_shader {
    unsigned int prog_id;

    int uniform_pos_offset;
    int uniform_extents;
    int uniform_view_proj_mat;
} te_debug_drawer_aabb_shader;

// Data for the line shader program.
typedef struct te_debug_drawer_line_shader {
    unsigned int prog_id;

    int uniform_from;
    int uniform_to;
    int uniform_view_proj_mat;
} te_debug_drawer_line_shader;

// Groups debug drawer data.
typedef struct te_debug_drawer {
    // Do not free this pointer.
    te_renderer* renderer;

    // Array of text to draw. Size of this array is @ref text_count.
    te_debug_drawer_text* texts;

    // Array of AABBs to draw. Size of this array is @ref aabb_count.
    te_debug_drawer_aabb* aabbs;

    // Array of lines to draw. Size of this array is @ref line_count.
    te_debug_drawer_line* lines;

    // Size of the array @ref texts.
    unsigned int text_count;

    // Size of the array @ref aabbs.
    unsigned int aabb_count;

    // Size of the array @ref lines.
    unsigned int line_count;

    // Quad geometry.
#if !defined(ENGINE_GLES)
    unsigned int vao_quad;
#endif
    unsigned int vbo_quad;
    unsigned int ebo_quad;

    // AABB geometry.
#if !defined(ENGINE_GLES)
    unsigned int vao_aabb;
#endif
    unsigned int vbo_aabb;
    unsigned int ebo_aabb;

    // Line geometry.
#if !defined(ENGINE_GLES)
    unsigned int vao_line;
#endif
    unsigned int vbo_line;
    unsigned int ebo_line;

    te_debug_drawer_text_shader text_shader;
    te_debug_drawer_aabb_shader aabb_shader;
    te_debug_drawer_line_shader line_shader;
} te_debug_drawer;

// Static to allow drawing easily from various places.
static te_debug_drawer drawer;

void
prv_debug_drawer_init(struct te_renderer* renderer) {
    drawer.texts = NULL;
    drawer.aabbs = NULL;
    drawer.lines = NULL;
    drawer.text_count = 0;
    drawer.aabb_count = 0;
    drawer.line_count = 0;

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

    // Load text shader.
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

    // Load AABB shader.
    {
        te_shader_manager* shader_manager = renderer_get_shader_manager(renderer);
        drawer.aabb_shader.prog_id = shader_manager_request_shader(
            shader_manager, "engine/shader/debug/aabb.vert.glsl",
            "engine/shader/debug/aabb.frag.glsl");

        drawer.aabb_shader.uniform_pos_offset =
            get_uniform_location(drawer.aabb_shader.prog_id, "pos_offset");
        drawer.aabb_shader.uniform_extents =
            get_uniform_location(drawer.aabb_shader.prog_id, "extents");
        drawer.aabb_shader.uniform_view_proj_mat =
            get_uniform_location(drawer.aabb_shader.prog_id, "view_proj_mat");
    }

    // Load line shader.
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

    // Create quad geometry.
    {
        vec4 vertices[4]; // XY pos, ZW uv
        glm_vec4_copy((vec4){0.0f, 0.0f, 0.0f, 0.0f}, &vertices[0][0]);
        glm_vec4_copy((vec4){0.0f, 1.0f, 0.0f, 1.0f}, &vertices[1][0]);
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, &vertices[2][0]);
        glm_vec4_copy((vec4){1.0f, 0.0f, 1.0f, 0.0f}, &vertices[3][0]);
        const unsigned short indices[6] = {0, 1, 2, 0, 2, 3};

#if !defined(ENGINE_GLES)
        glGenVertexArrays(1, &drawer.vao_quad);
#endif
        glGenBuffers(1, &drawer.vbo_quad);
        glGenBuffers(1, &drawer.ebo_quad);

#if !defined(ENGINE_GLES)
        glBindVertexArray(drawer.vao_quad);
#endif

        glBindBuffer(GL_ARRAY_BUFFER, drawer.vbo_quad);
        glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(vec4), &vertices[0][0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer.ebo_quad);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

#if defined(ENGINE_GLES)
        glBindAttribLocation(drawer.text_shader.prog_id, 0, "vertex");
#endif
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), NULL);

#if !defined(ENGINE_GLES)
        glBindVertexArray(0);
#endif
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    // Create AABB geometry (using lines).
    {
        vec3 extents;
        glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, extents);

        vec3 vertices[8];

        glm_vec3_copy((vec3){-extents[0], -extents[1], -extents[2]}, &vertices[0][0]);
        glm_vec3_copy((vec3){+extents[0], -extents[1], -extents[2]}, &vertices[1][0]);
        glm_vec3_copy((vec3){+extents[0], -extents[1], +extents[2]}, &vertices[2][0]);
        glm_vec3_copy((vec3){-extents[0], -extents[1], +extents[2]}, &vertices[3][0]);

        glm_vec3_copy((vec3){-extents[0], +extents[1], -extents[2]}, &vertices[4][0]);
        glm_vec3_copy((vec3){+extents[0], +extents[1], -extents[2]}, &vertices[5][0]);
        glm_vec3_copy((vec3){+extents[0], +extents[1], +extents[2]}, &vertices[6][0]);
        glm_vec3_copy((vec3){-extents[0], +extents[1], +extents[2]}, &vertices[7][0]);

        const unsigned short indices[TE_DEBUG_DRAWER_AABB_INDEX_COUNT] = {
            0, 1, 1, 2, 2, 3, 3, 0, // lower quad
            4, 5, 5, 6, 6, 7, 7, 4, // upper quad
            0, 4, 1, 5, 2, 6, 3, 7  // vertical lines
        };

#if !defined(ENGINE_GLES)
        glGenVertexArrays(1, &drawer.vao_aabb);
#endif
        glGenBuffers(1, &drawer.vbo_aabb);
        glGenBuffers(1, &drawer.ebo_aabb);

#if !defined(ENGINE_GLES)
        glBindVertexArray(drawer.vao_aabb);
#endif

        glBindBuffer(GL_ARRAY_BUFFER, drawer.vbo_aabb);
        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(vec3), &vertices[0][0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer.ebo_aabb);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER, 24 * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

#if defined(ENGINE_GLES)
        glBindAttribLocation(drawer.aabb_shader.prog_id, 0, "local_pos");
#endif
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), NULL);

#if !defined(ENGINE_GLES)
        glBindVertexArray(0);
#endif
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    // Create line geometry.
    {
        vec3 vertices[2];

        glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, &vertices[0][0]);
        glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, &vertices[1][0]);

        const unsigned short indices[2] = {0, 1};

#if !defined(ENGINE_GLES)
        glGenVertexArrays(1, &drawer.vao_line);
#endif
        glGenBuffers(1, &drawer.vbo_line);
        glGenBuffers(1, &drawer.ebo_line);

#if !defined(ENGINE_GLES)
        glBindVertexArray(drawer.vao_line);
#endif

        glBindBuffer(GL_ARRAY_BUFFER, drawer.vbo_line);
        glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(vec3), &vertices[0][0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer.ebo_line);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER, 2 * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

#if defined(ENGINE_GLES)
        glBindAttribLocation(drawer.line_shader.prog_id, 0, "local_pos");
#endif
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), NULL);

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
    // Free debug objects.
    for (unsigned int i = 0; i < drawer.text_count; i++) {
        prv_debug_drawer_free_text(&drawer.texts[i]);
    }
    free(drawer.texts);
    drawer.texts = NULL;
    drawer.text_count = 0;

    free(drawer.aabbs);
    drawer.aabbs = NULL;
    drawer.aabb_count = 0;

    free(drawer.lines);
    drawer.lines = NULL;
    drawer.line_count = 0;

    // Free shaders.
    te_shader_manager* shader_manager = renderer_get_shader_manager(renderer);
    shader_manager_mark_unused_shader(shader_manager, drawer.text_shader.prog_id);
    shader_manager_mark_unused_shader(shader_manager, drawer.aabb_shader.prog_id);
    shader_manager_mark_unused_shader(shader_manager, drawer.line_shader.prog_id);

    // Free geometry.
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
debug_drawer_draw_aabb(te_aabb_shape* aabb, float time_sec) {
    if (drawer.renderer == NULL) {
        return;
    }

    te_debug_drawer_aabb* aabbs =
        malloc(sizeof(te_debug_drawer_aabb) * (drawer.aabb_count + 1));
    memcpy(aabbs, drawer.aabbs, sizeof(te_debug_drawer_aabb) * drawer.aabb_count);

    free(drawer.aabbs);
    drawer.aabbs = aabbs;

    te_debug_drawer_aabb* new_item = &drawer.aabbs[drawer.aabb_count];
    new_item->aabb = *aabb;
    new_item->time_left_sec = time_sec;

    drawer.aabb_count += 1;
}

void
debug_drawer_draw_line(vec3 from, vec3 to, float time_sec) {
    if (drawer.renderer == NULL) {
        return;
    }

    te_debug_drawer_line* lines =
        malloc(sizeof(te_debug_drawer_line) * (drawer.line_count + 1));
    memcpy(lines, drawer.lines, sizeof(te_debug_drawer_line) * drawer.line_count);

    free(drawer.lines);
    drawer.lines = lines;

    te_debug_drawer_line* new_item = &drawer.lines[drawer.line_count];
    new_item->time_left_sec = time_sec;
    glm_vec3_copy(from, new_item->from);
    glm_vec3_copy(to, new_item->to);

    drawer.line_count += 1;
}

void
debug_drawer_draw_text_fmt(float time_sec, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);

    int test_size = vsnprintf(NULL, 0, fmt, args);
    if (test_size <= 0) {
        log_error("failed to format last log message");
        abort();
    }
    size_t size = (size_t)test_size;
    char* message = malloc(size + 1);
    memset(message, 0, size + 1);

    vsprintf(message, fmt, args_copy);

    va_end(args_copy);
    va_end(args);

    debug_drawer_draw_text_color(message, time_sec, (vec3){1.0f, 1.0f, 1.0f});

    free(message);
}

void
debug_drawer_draw_text_color(const char* text, float time_sec, vec3 color) {
    debug_drawer_draw_text_color_pos(text, time_sec, color, (vec2){-1.0f, -1.0f});
}

void
debug_drawer_draw_text_color_pos(const char* text, float time_sec, vec3 color, vec2 pos) {
    if (drawer.renderer == NULL) {
        return;
    }

    te_debug_drawer_text* new_texts =
        malloc(sizeof(te_debug_drawer_text) * (drawer.text_count + 1));
    memcpy(new_texts, drawer.texts, sizeof(te_debug_drawer_text) * drawer.text_count);

    free(drawer.texts);
    drawer.texts = new_texts;

    // Init data.
    te_debug_drawer_text* new_item = &drawer.texts[drawer.text_count];
    glm_vec3_copy(color, new_item->color);
    new_item->color[3] = 1.0f;
    glm_vec2_copy(pos, new_item->pos);
    new_item->time_left_sec = time_sec;

    // Copy text.
    const size_t text_len = strlen(text);
    new_item->text = malloc(sizeof(char) * (text_len + 1));
    memcpy(new_item->text, text, sizeof(char) * text_len);
    new_item->text[text_len] = 0;
    new_item->text_len = (unsigned int)text_len;

    // Cache glyphs.
    te_font_manager* font_manager = renderer_get_font_manager(drawer.renderer);
    const float font_height = prv_font_manager_get_font_height_to_load();
    const float font_scale = debug_drawer_default_text_height / font_height;
    new_item->glyphs = malloc(sizeof(te_debug_drawer_glyph) * new_item->text_len);
    for (unsigned int i = 0; i < new_item->text_len; i++) {
        te_font_glyph src =
            font_manager_get_glyph(font_manager, (unsigned long)new_item->text[i]);
        te_debug_drawer_glyph* dst = &new_item->glyphs[i];

        dst->distance_to_next_glyph =
            (float)(src.advance >> 6) // bitshift by 6 to get value in pixels (2^6 = 64)
            * font_scale;

        if (src.width == 0) {
            dst->tex_id = 0;
        } else {
            dst->tex_id = src.tex_id;
            glm_vec2_copy(
                (vec2){(float)src.bearing_x * font_scale, -(float)src.bearing_y * font_scale},
                dst->pos_offset);
            glm_vec2_copy(
                (vec2){(float)src.width * font_scale, (float)src.height * font_scale},
                dst->size);
        }
    }

    drawer.text_count += 1;
}

float
debug_drawer_get_default_text_height() {
    return debug_drawer_default_text_height;
}

void
prv_debug_drawer_draw(
    struct te_renderer* renderer, float delta_time_sec, mat4* view_proj_mat) {
    unsigned int window_width;
    unsigned int window_height;
    window_get_size(renderer_get_window(renderer), &window_width, &window_height);
    vec2 window_size;
    glm_vec2_copy((vec2){(float)window_width, (float)window_height}, window_size);

    glDisable(GL_DEPTH_TEST);

    if (drawer.line_count > 0) {
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

        for (unsigned int i = 0; i < drawer.line_count;) {
            te_debug_drawer_line* line = &drawer.lines[i];

            glUniform3fv(drawer.line_shader.uniform_from, 1, line->from);
            glUniform3fv(drawer.line_shader.uniform_to, 1, line->to);

            glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, NULL);

            // Update state.
            line->time_left_sec -= delta_time_sec;
            if (line->time_left_sec < 0.0f) {
                // No longer render this item.
                // Note: nothing to free inside of the line item.
                if (drawer.line_count == 1) {
                    free(drawer.lines);
                    drawer.lines = NULL;
                } else {
                    te_debug_drawer_line* new_lines =
                        malloc(sizeof(te_debug_drawer_line) * (drawer.line_count - 1));
                    memcpy(new_lines, drawer.lines, sizeof(te_debug_drawer_line) * i);
                    memcpy(
                        new_lines + i, drawer.lines + (i + 1),
                        sizeof(te_debug_drawer_line) * (drawer.line_count - i - 1));
                    free(drawer.lines);
                    drawer.lines = new_lines;
                }
                drawer.line_count -= 1;
            } else {
                i += 1;
            }
        }
    }

    if (drawer.aabb_count > 0) {
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

        for (unsigned int i = 0; i < drawer.aabb_count;) {
            te_debug_drawer_aabb* wireframe = &drawer.aabbs[i];

            glUniform3fv(drawer.aabb_shader.uniform_pos_offset, 1, wireframe->aabb.center);
            glUniform3fv(drawer.aabb_shader.uniform_extents, 1, wireframe->aabb.extents);

            glDrawElements(
                GL_LINES, TE_DEBUG_DRAWER_AABB_INDEX_COUNT, GL_UNSIGNED_SHORT, NULL);

            // Update state.
            wireframe->time_left_sec -= delta_time_sec;
            if (wireframe->time_left_sec < 0.0f) {
                // No longer render this item.
                // Note: nothing to free inside of the AABB item.
                if (drawer.aabb_count == 1) {
                    free(drawer.aabbs);
                    drawer.aabbs = NULL;
                } else {
                    te_debug_drawer_aabb* new_wireframes =
                        malloc(sizeof(te_debug_drawer_aabb) * (drawer.aabb_count - 1));
                    memcpy(new_wireframes, drawer.aabbs, sizeof(te_debug_drawer_aabb) * i);
                    memcpy(
                        new_wireframes + i, drawer.aabbs + (i + 1),
                        sizeof(te_debug_drawer_aabb) * (drawer.aabb_count - i - 1));
                    free(drawer.aabbs);
                    drawer.aabbs = new_wireframes;
                }
                drawer.aabb_count -= 1;
            } else {
                i += 1;
            }
        }
    }

    if (drawer.text_count > 0) {
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

        glActiveTexture(GL_TEXTURE0); // glyph's bitmap

        vec4 clip_rect;
        glm_vec4_copy((vec4){0.0f, 0.0f, 1.0f, 1.0f}, clip_rect);
        glUniform4fv(drawer.text_shader.uniform_clip_rect, 1, clip_rect);

        glUniform2fv(drawer.text_shader.uniform_window_size, 1, window_size);

        // Prepare starting position for the first text (relative to screen's top-left corner).
        // x will be reset on every text so it's defined below.
        vec2 screen_pos;
        screen_pos[1] = (float)window_height * 0.1f;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (unsigned int i = 0; i < drawer.text_count;) {
            te_debug_drawer_text* text = &drawer.texts[i];

            screen_pos[0] = window_size[0] * 0.025f;
            if (text->pos[0] >= 0.0f) {
                screen_pos[0] = window_size[0] * text->pos[0];
            }
            const float text_height = window_size[1] * font_height * font_scale;

            // Switch to the first row of the text.
            const float auto_screen_y = screen_pos[1]; // save to restore later
            if (text->pos[1] >= 0.0f) {
                screen_pos[1] = window_size[1] * text->pos[1];
            }
            screen_pos[1] += text_height;

            glUniform4fv(drawer.text_shader.uniform_text_color, 1, text->color);

            // Draw each glyph.
            for (unsigned int i = 0; i < text->text_len; i++) {
                if (text->glyphs[i].tex_id == 0) {
                    screen_pos[0] += text->glyphs[i].distance_to_next_glyph;
                } else {
                    vec2 glyph_pos;
                    glm_vec2_add(screen_pos, text->glyphs[i].pos_offset, glyph_pos);

                    glUniform2fv(drawer.text_shader.uniform_in_pos, 1, glyph_pos);
                    glUniform2fv(drawer.text_shader.uniform_in_size, 1, text->glyphs[i].size);

                    glBindTexture(GL_TEXTURE_2D, text->glyphs[i].tex_id);

                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
                }
                screen_pos[0] += text->glyphs[i].distance_to_next_glyph;
            }

            // Restore Y.
            if (text->pos[1] > 0.0f) {
                screen_pos[1] = auto_screen_y;
            }

            // Update state.
            text->time_left_sec -= delta_time_sec;
            if (text->time_left_sec < 0.0f) {
                // No longer render this text.
                prv_debug_drawer_free_text(text);
                if (drawer.text_count == 1) {
                    free(drawer.texts);
                    drawer.texts = NULL;
                } else {
                    te_debug_drawer_text* new_texts =
                        malloc(sizeof(te_debug_drawer_text) * (drawer.text_count - 1));
                    memcpy(new_texts, drawer.texts, sizeof(te_debug_drawer_text) * i);
                    memcpy(
                        new_texts + i, drawer.texts + (i + 1),
                        sizeof(te_debug_drawer_text) * (drawer.text_count - i - 1));
                    free(drawer.texts);
                    drawer.texts = new_texts;
                }
                drawer.text_count -= 1;
            } else {
                i += 1;
            }
        }
        glDisable(GL_BLEND);
    }

    glEnable(GL_DEPTH_TEST);
}

#endif
