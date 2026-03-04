#include <render/debug_drawer.h>

#if defined(ENGINE_DEBUG_TOOLS)

#include <stdlib.h>
#include <string.h>
#include <render/font_manager.h>
#include <render/renderer.h>
#include <render/shader_manager.h>
#include <window.h>
#include <glad/glad.h>

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

// Groups data related to the text shader program.
typedef struct te_debug_drawer_text_shader {
    unsigned int prog_id;

    int uniform_in_pos;
    int uniform_in_size;
    int uniform_clip_rect;
    int uniform_window_size;
    int uniform_text_color;
} te_debug_drawer_text_shader;

// Groups debug drawer data.
typedef struct te_debug_drawer {
    // Do not free this pointer.
    te_renderer* renderer;

    // Array of text to draw. Size of this array is @ref text_count.
    te_debug_drawer_text* texts;

    // Size of the array @ref texts.
    unsigned int text_count;

    // Quad geometry.
    unsigned int vbo;
    unsigned int ebo;

    te_debug_drawer_text_shader text_shader;
} te_debug_drawer;

// Static to allow drawing easily from various places.
static te_debug_drawer drawer;

void
prv_debug_drawer_init(struct te_renderer* renderer) {
    drawer.texts = NULL;
    drawer.text_count = 0;
    drawer.renderer = renderer;

    // Load text shader.
    {
        te_shader_manager* shader_manager = renderer_get_shader_manager(renderer);
        drawer.text_shader.prog_id =
            shader_manager_request_shader(shader_manager, "engine/shader/quad.vert.glsl", "engine/shader/text.frag.glsl");

        drawer.text_shader.uniform_in_pos = get_uniform_location(drawer.text_shader.prog_id, "in_pos");
        drawer.text_shader.uniform_in_size = get_uniform_location(drawer.text_shader.prog_id, "in_size");
        drawer.text_shader.uniform_clip_rect = get_uniform_location(drawer.text_shader.prog_id, "clip_rect");
        drawer.text_shader.uniform_window_size = get_uniform_location(drawer.text_shader.prog_id, "window_size");
        drawer.text_shader.uniform_text_color = get_uniform_location(drawer.text_shader.prog_id, "text_color");
    }

    // Create quad geometry.
    {
        vec4 vertices[4]; // XY pos, ZW uv
        glm_vec4_copy((vec4){0.0f, 0.0f, 0.0f, 0.0f}, &vertices[0][0]);
        glm_vec4_copy((vec4){0.0f, 1.0f, 0.0f, 1.0f}, &vertices[1][0]);
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, &vertices[2][0]);
        glm_vec4_copy((vec4){1.0f, 0.0f, 1.0f, 0.0f}, &vertices[3][0]);
        const unsigned short indices[6] = {0, 1, 2, 0, 2, 3};

        glGenBuffers(1, &drawer.vbo);
        glGenBuffers(1, &drawer.ebo);

        glBindBuffer(GL_ARRAY_BUFFER, drawer.vbo);
        glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(vec4), &vertices[0][0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

        glBindAttribLocation(drawer.text_shader.prog_id, 0, "vertex");
        glEnableVertexAttribArray(0);

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
    drawer.text_count = 0;

    // Free shaders.
    te_shader_manager* shader_manager = renderer_get_shader_manager(renderer);
    shader_manager_mark_unused_shader(shader_manager, drawer.text_shader.prog_id);

    // Free geometry.
    glDeleteBuffers(1, &drawer.vbo);
    glDeleteBuffers(1, &drawer.ebo);

    drawer.renderer = NULL;
}

void
debug_drawer_draw_text(const char* text, float time_sec) {
    debug_drawer_draw_text_color(text, time_sec, (vec3){1.0f, 1.0f, 1.0f});
}

void
debug_drawer_draw_text_color(const char* text, float time_sec, vec3 color) {
    debug_drawer_draw_text_color_pos(text, time_sec, color, (vec2){-1.0f, -1.0f});
}

void
debug_drawer_draw_text_color_pos(const char* text, float time_sec, vec3 color, vec2 pos) {
    te_debug_drawer_text* new_texts = malloc(sizeof(te_debug_drawer_text) * (drawer.text_count + 1));
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
        te_font_glyph src = font_manager_get_glyph(font_manager, (unsigned long)new_item->text[i]);
        te_debug_drawer_glyph* dst = &new_item->glyphs[i];

        dst->distance_to_next_glyph = (float)(src.advance >> 6) // bitshift by 6 to get value in pixels (2^6 = 64)
                                      * font_scale;

        if (src.width == 0) {
            dst->tex_id = 0;
        } else {
            dst->tex_id = src.tex_id;
            glm_vec2_copy((vec2){(float)src.bearing_x * font_scale, -(float)src.bearing_y * font_scale}, dst->pos_offset);
            glm_vec2_copy((vec2){(float)src.width * font_scale, (float)src.height * font_scale}, dst->size);
        }
    }

    drawer.text_count += 1;
}

float
debug_drawer_get_default_text_height() {
    return debug_drawer_default_text_height;
}

void
prv_debug_drawer_draw(struct te_renderer* renderer, float delta_time_sec) {
    unsigned int window_width;
    unsigned int window_height;
    window_get_size(renderer_get_window(renderer), &window_width, &window_height);
    vec2 window_size;
    glm_vec2_copy((vec2){(float)window_width, (float)window_height}, window_size);

    glDisable(GL_DEPTH_TEST);

    if (drawer.text_count > 0) {
        const float font_height = prv_font_manager_get_font_height_to_load();
        const float font_scale = debug_drawer_default_text_height / font_height;

        glUseProgram(drawer.text_shader.prog_id);
        glBindBuffer(GL_ARRAY_BUFFER, drawer.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer.ebo);
        glActiveTexture(GL_TEXTURE0); // glyph's bitmap

        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), NULL);

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
                    te_debug_drawer_text* new_texts = malloc(sizeof(te_debug_drawer_text) * (drawer.text_count - 1));
                    memcpy(new_texts, drawer.texts, sizeof(te_debug_drawer_text) * i);
                    memcpy(new_texts + i, drawer.texts + (i + 1), sizeof(te_debug_drawer_text) * (drawer.text_count - i - 1));
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
