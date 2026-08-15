#include <render/widget_renderer.h>

#include <string.h>
#include <io/log.h>
#include <render/gpu_section.h>
#include <render/renderer.h>
#include <render/shader_manager.h>
#include <render/render_data_array.h>
#include <window.h>
#include <glad/glad.h>

#define WIDGET_QUAD_GL_VERT_ATTRIB_PTR                                                        \
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), NULL);

// Groups data related to the text shader program.
typedef struct te_text_shader_data {
    unsigned int prog_id;

    int uniform_in_pos;
    int uniform_in_size;
    int uniform_clip_rect;
    int uniform_window_size;
    int uniform_text_color;
} te_text_shader_data;

// Groups data related to the quad shader program.
typedef struct te_quad_shader_data {
    unsigned int prog_id;

    int uniform_in_pos;
    int uniform_in_size;
    int uniform_clip_rect;
    int uniform_window_size;
    int uniform_is_using_tex;
    int uniform_quad_color;
} te_quad_shader_data;

struct te_widget_renderer {
    te_render_data_array* text_widget_data;
    te_render_data_array* rect_widget_data;

    // Do not free/destroy this pointer.
    te_renderer* renderer;

    // Quad geometry.
#if !defined(ENGINE_GLES)
    unsigned int vao;
#endif
    unsigned int vbo;
    unsigned int ebo;

    te_text_shader_data text_shader;
    te_quad_shader_data quad_shader;
};

te_widget_renderer*
widget_renderer_create(te_renderer* renderer) {
    te_widget_renderer* widget_renderer = malloc(sizeof(te_widget_renderer));

    widget_renderer->text_widget_data =
        render_data_array_create(sizeof(te_text_widget_render_data), 64, 64);
    widget_renderer->rect_widget_data =
        render_data_array_create(sizeof(te_rect_widget_render_data), 32, 32);
    widget_renderer->renderer = renderer;

    // Load text shader.
    te_shader_manager* shader_manager = renderer_get_shader_manager(renderer);
    {
        te_text_shader_data* shader = &widget_renderer->text_shader;

        shader->prog_id = shader_manager_request_shader(
            shader_manager, "engine/shader/quad.vert.glsl", "engine/shader/text.frag.glsl");

        shader->uniform_in_pos = get_uniform_location(shader->prog_id, "in_pos");
        shader->uniform_in_size = get_uniform_location(shader->prog_id, "in_size");
        shader->uniform_clip_rect = get_uniform_location(shader->prog_id, "clip_rect");
        shader->uniform_window_size = get_uniform_location(shader->prog_id, "window_size");
        shader->uniform_text_color = get_uniform_location(shader->prog_id, "text_color");
    }

    // Load quad shader.
    {
        te_quad_shader_data* shader = &widget_renderer->quad_shader;

        shader->prog_id = shader_manager_request_shader(
            shader_manager, "engine/shader/quad.vert.glsl", "engine/shader/quad.frag.glsl");

        shader->uniform_in_pos = get_uniform_location(shader->prog_id, "in_pos");
        shader->uniform_in_size = get_uniform_location(shader->prog_id, "in_size");
        shader->uniform_clip_rect = get_uniform_location(shader->prog_id, "clip_rect");
        shader->uniform_window_size = get_uniform_location(shader->prog_id, "window_size");
        shader->uniform_is_using_tex = get_uniform_location(shader->prog_id, "is_using_tex");
        shader->uniform_quad_color = get_uniform_location(shader->prog_id, "quad_color");
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
        glGenVertexArrays(1, &widget_renderer->vao);
#endif
        glGenBuffers(1, &widget_renderer->vbo);
        glGenBuffers(1, &widget_renderer->ebo);

#if !defined(ENGINE_GLES)
        glBindVertexArray(widget_renderer->vao);
#endif
        {
            // Vertex buffer.
            glBindBuffer(GL_ARRAY_BUFFER, widget_renderer->vbo);
            glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(vec4), &vertices[0][0], GL_STATIC_DRAW);

            // Vertex layout.
            glEnableVertexAttribArray(0);
            WIDGET_QUAD_GL_VERT_ATTRIB_PTR

            // Index buffer.
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, widget_renderer->ebo);
            glBufferData(
                GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(unsigned short), &indices[0],
                GL_STATIC_DRAW);
        }
#if !defined(ENGINE_GLES)
        glBindVertexArray(0);
#endif
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    return widget_renderer;
}

void
widget_renderer_destroy(te_widget_renderer* widget_renderer) {
    if (render_data_array_get_item_count(widget_renderer->text_widget_data) > 0) {
        log_error("widget renderer is being destroyed but there are still some text widgets "
                  "rendering");
        abort();
    }
    render_data_array_destroy(widget_renderer->text_widget_data);

    if (render_data_array_get_item_count(widget_renderer->rect_widget_data) > 0) {
        log_error("widget renderer is being destroyed but there are still some rect widgets "
                  "rendering");
        abort();
    }
    render_data_array_destroy(widget_renderer->rect_widget_data);

    shader_manager_mark_unused_shader(
        renderer_get_shader_manager(widget_renderer->renderer),
        widget_renderer->text_shader.prog_id);
    shader_manager_mark_unused_shader(
        renderer_get_shader_manager(widget_renderer->renderer),
        widget_renderer->quad_shader.prog_id);

#if !defined(ENGINE_GLES)
    glDeleteVertexArrays(1, &widget_renderer->vao);
#endif
    glDeleteBuffers(1, &widget_renderer->vbo);
    glDeleteBuffers(1, &widget_renderer->ebo);

    free(widget_renderer);
}

unsigned int
widget_renderer_add_text_widget(te_widget_renderer* renderer) {
    const unsigned int handle = render_data_array_add_item(renderer->text_widget_data);

    // Init data.
    te_text_widget_render_data* data =
        render_data_array_get_item_data_tmp(renderer->text_widget_data, handle);
    memset(data, 0, sizeof(te_text_widget_render_data));

    return handle;
}

te_text_widget_render_data*
widget_renderer_get_text_widget_render_data_tmp(
    te_widget_renderer* renderer, unsigned int handle) {
    return render_data_array_get_item_data_tmp(renderer->text_widget_data, handle);
}

te_rect_widget_render_data*
widget_renderer_get_rect_widget_render_data_tmp(
    te_widget_renderer* renderer, unsigned int handle) {
    return render_data_array_get_item_data_tmp(renderer->rect_widget_data, handle);
}

void
widget_renderer_remove_text_widget(te_widget_renderer* renderer, unsigned int handle) {
    // Cleanup.
    te_text_widget_render_data* data =
        render_data_array_get_item_data_tmp(renderer->text_widget_data, handle);
    free(data->glyphs);

    render_data_array_remove_item(renderer->text_widget_data, handle);
}

unsigned int
widget_renderer_add_rect_widget(te_widget_renderer* renderer) {
    const unsigned int handle = render_data_array_add_item(renderer->rect_widget_data);

    // Init data.
    te_rect_widget_render_data* data =
        render_data_array_get_item_data_tmp(renderer->rect_widget_data, handle);
    memset(data, 0, sizeof(te_rect_widget_render_data));

    return handle;
}

void
widget_renderer_remove_rect_widget(te_widget_renderer* renderer, unsigned int handle) {
    render_data_array_remove_item(renderer->rect_widget_data, handle);
}

void
widget_renderer_draw(te_widget_renderer* widget_renderer) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(
        renderer_get_window(widget_renderer->renderer), &window_width, &window_height);
    vec2 window_size;
    glm_vec2_copy((vec2){(float)window_width, (float)window_height}, window_size);

    // Draw rect widgets.
    const unsigned int rect_widget_count =
        render_data_array_get_item_count(widget_renderer->rect_widget_data);
    if (rect_widget_count > 0) {
        te_quad_shader_data* shader = &widget_renderer->quad_shader;
        GPU_SECTION_BEGIN("rect");

        glUseProgram(shader->prog_id);

#if defined(ENGINE_GLES)
        glBindBuffer(GL_ARRAY_BUFFER, widget_renderer->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, widget_renderer->ebo);
        WIDGET_QUAD_GL_VERT_ATTRIB_PTR
#else
        glBindVertexArray(widget_renderer->vao);
#endif

        glActiveTexture(GL_TEXTURE0); // quad texture

        glUniform2fv(shader->uniform_window_size, 1, window_size);

        te_rect_widget_render_data* data =
            render_data_array_get_internal_array(widget_renderer->rect_widget_data);

        for (unsigned int widget_idx = 0; widget_idx < rect_widget_count; widget_idx++) {
            glUniform4fv(shader->uniform_quad_color, 1, data->color);
            glUniform4fv(shader->uniform_clip_rect, 1, data->clip_rect);

            glUniform2fv(shader->uniform_in_pos, 1, data->pos_pix);
            glUniform2fv(shader->uniform_in_size, 1, data->size_pix);

            glUniform1i(shader->uniform_is_using_tex, data->tex_id > 0);
            glBindTexture(GL_TEXTURE_2D, data->tex_id);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

            data += 1;
        }

        GPU_SECTION_END;
    }

    // Draw text widgets.
    const unsigned int text_widget_count =
        render_data_array_get_item_count(widget_renderer->text_widget_data);
    if (text_widget_count > 0) {
        te_text_shader_data* shader = &widget_renderer->text_shader;
        GPU_SECTION_BEGIN("text");

        glUseProgram(shader->prog_id);

#if defined(ENGINE_GLES)
        glBindBuffer(GL_ARRAY_BUFFER, widget_renderer->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, widget_renderer->ebo);
        WIDGET_QUAD_GL_VERT_ATTRIB_PTR
#else
        glBindVertexArray(widget_renderer->vao);
#endif

        glActiveTexture(GL_TEXTURE0); // glyph's bitmap

        vec4 clip_rect;
        glm_vec4_copy((vec4){0.0f, 0.0f, 1.0f, 1.0f}, clip_rect);
        glUniform4fv(shader->uniform_clip_rect, 1, clip_rect);

        glUniform2fv(shader->uniform_window_size, 1, window_size);

        te_text_widget_render_data* data =
            render_data_array_get_internal_array(widget_renderer->text_widget_data);
        vec2 pos_pix;

        for (unsigned int widget_idx = 0; widget_idx < text_widget_count; widget_idx++) {
            glUniform4fv(shader->uniform_text_color, 1, data->color);

            for (unsigned int glyph_idx = 0; glyph_idx < data->glyph_count; glyph_idx++) {
                glm_vec2_add(data->pos_pix, data->glyphs[glyph_idx].offset_pix, pos_pix);

                glUniform2fv(shader->uniform_in_pos, 1, pos_pix);
                glUniform2fv(shader->uniform_in_size, 1, data->glyphs[glyph_idx].size_pix);

                glBindTexture(GL_TEXTURE_2D, data->glyphs[glyph_idx].tex_id);

                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
            }

            data += 1;
        }

        GPU_SECTION_END;
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
