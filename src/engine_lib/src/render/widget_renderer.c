#include "render/widget_renderer.h"

#include <string.h>
#include "io/log.h"
#include "render/gpu_section.h"
#include "render/renderer.h"
#include "render/shader_manager.h"
#include "window.h"
#include "glad/glad.h"

#define INVALID_DATA_INDEX 0xffffffff

// Groups render data related to a specific type of a widget.
typedef struct te_widgets_render_data {
    // Array of render data of all currently registered items.
    //
    // Max size of this array is @ref array_size but the actual number of valid
    // (used) elements might be different (see @ref render_data_count). When some widget's render data is removed all next
    // elements are shifted to the left to make sure the array does not have any "holes".
    // This array does not shrink.
    void* render_data;

    // Index into this array using a widget's handle to get index into @ref render_data.
    //
    // Public API users store indices into this array so items cannot be reordered/moved.
    // This array CAN have "holes" in it (invalid items). Invalid items store INVALID_DATA_INDEX value.
    // This array does not shrink. Size of this array is @ref array_size.
    unsigned int* handle_to_data;

    // Max number of elements in the arrays @ref render_data and @ref handle_to_data.
    unsigned int array_size;

    // sizeof for a single render data item.
    unsigned int sizeof_render_data;

    // Actual number of used (valid) elements in @ref render_data.
    unsigned int render_data_count;
} te_widgets_render_data;

static te_widgets_render_data*
widgets_render_data_create(unsigned int sizeof_render_data) {
    te_widgets_render_data* data = malloc(sizeof(te_widgets_render_data));

    data->sizeof_render_data = sizeof_render_data;
    data->array_size = 4;
    data->render_data = malloc(sizeof_render_data * data->array_size);
    data->handle_to_data = malloc(sizeof(unsigned int) * data->array_size);
    for (unsigned int i = 0; i < data->array_size; i++) {
        data->handle_to_data[i] = INVALID_DATA_INDEX;
    }
    data->render_data_count = 0;

    return data;
}

static void
widgets_render_data_destroy(te_widgets_render_data* data) {
    free(data->render_data);
    free(data->handle_to_data);
    free(data);
}

static unsigned int
widgets_render_data_add_widget(te_widgets_render_data* data) {
    // Find unused handle.
    unsigned int handle = 0;
    bool found = false;
    for (unsigned int i = 0; i < data->array_size; i++) {
        if (data->handle_to_data[i] != INVALID_DATA_INDEX) {
            continue;
        }
        handle = i;
        found = true;
        break;
    }
    if (!found) {
        // Expand array.
        const unsigned int expand_size = 4;

        unsigned int* new_handles = malloc(sizeof(unsigned int) * (data->array_size + expand_size));
        memcpy(new_handles, data->handle_to_data, sizeof(unsigned int) * data->array_size);

        free(data->handle_to_data);
        data->handle_to_data = new_handles;

        void* new_data = malloc(data->sizeof_render_data * (data->array_size + expand_size));
        memcpy(new_data, data->render_data, data->sizeof_render_data * data->render_data_count);

        free(data->render_data);
        data->render_data = new_data;

        for (unsigned int i = data->array_size; i < data->array_size + expand_size; i++) {
            data->handle_to_data[i] = INVALID_DATA_INDEX;
        }

        handle = data->array_size;
        data->array_size += expand_size;
    }

    data->handle_to_data[handle] = data->render_data_count;
    data->render_data_count += 1;

    return handle;
}

static void
widgets_render_data_remove_widget(te_widgets_render_data* data, unsigned int handle) {
#if defined(DEBUG)
    if (handle >= data->array_size) {
        log_error("the specified widget handle is invalid");
        abort();
    }
#endif

    const unsigned int render_data_index = data->handle_to_data[handle];

#if defined(DEBUG)
    if (render_data_index == INVALID_DATA_INDEX) {
        log_error("the specified widget handle is invalid");
        abort();
    }
#endif

    data->handle_to_data[handle] = INVALID_DATA_INDEX;

    if (data->render_data_count > 1) {
        // Remove "hole" from the array.
        char* render_data = data->render_data; // <- cast from void*
        memmove(
            render_data + data->sizeof_render_data * render_data_index,
            render_data + data->sizeof_render_data * (render_data_index + 1),
            data->sizeof_render_data * (data->render_data_count - render_data_index - 1));
    }

    // Shift render data indices after the removed one.
    for (unsigned int i = 0; i < data->array_size; i++) {
        if (data->handle_to_data[i] == INVALID_DATA_INDEX || data->handle_to_data[i] < render_data_index) {
            continue;
        }
        data->handle_to_data[i] -= 1;
    }

    data->render_data_count -= 1;
}

void*
widgets_render_data_get_widget_data(te_widgets_render_data* data, unsigned int handle) {
    char* render_data = data->render_data; // <- cast from void*
    return render_data + data->sizeof_render_data * data->handle_to_data[handle];
}

unsigned int
widgets_render_data_count(te_widgets_render_data* data) {
    return data->render_data_count;
}

void*
widgets_render_data_get(te_widgets_render_data* data) {
    return data->render_data;
}

// ----------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------

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
    te_widgets_render_data* text_widget_data;

    te_widgets_render_data* rect_widget_data;

    // Do not free/destroy this pointer.
    te_renderer* renderer;

    // Quad geometry.
    unsigned int vbo;
    unsigned int ebo;

    te_text_shader_data text_shader;
    te_quad_shader_data quad_shader;
};

te_widget_renderer*
widget_renderer_create(te_renderer* renderer) {
    te_widget_renderer* widget_renderer = malloc(sizeof(te_widget_renderer));

    widget_renderer->text_widget_data = widgets_render_data_create(sizeof(te_text_widget_render_data));
    widget_renderer->rect_widget_data = widgets_render_data_create(sizeof(te_rect_widget_render_data));
    widget_renderer->renderer = renderer;

    // Load text shader.
    te_shader_manager* shader_manager = renderer_get_shader_manager(renderer);
    {
        te_text_shader_data* shader = &widget_renderer->text_shader;

        shader->prog_id =
            shader_manager_request_shader(shader_manager, "engine/shader/quad.vert.glsl", "engine/shader/text.frag.glsl");

        shader->uniform_in_pos = get_uniform_location(shader->prog_id, "in_pos");
        shader->uniform_in_size = get_uniform_location(shader->prog_id, "in_size");
        shader->uniform_clip_rect = get_uniform_location(shader->prog_id, "clip_rect");
        shader->uniform_window_size = get_uniform_location(shader->prog_id, "window_size");
        shader->uniform_text_color = get_uniform_location(shader->prog_id, "text_color");
    }

    // Load quad shader.
    {
        te_quad_shader_data* shader = &widget_renderer->quad_shader;

        shader->prog_id =
            shader_manager_request_shader(shader_manager, "engine/shader/quad.vert.glsl", "engine/shader/quad.frag.glsl");

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

        glGenBuffers(1, &widget_renderer->vbo);
        glGenBuffers(1, &widget_renderer->ebo);

        glBindBuffer(GL_ARRAY_BUFFER, widget_renderer->vbo);
        glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(vec4), &vertices[0][0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, widget_renderer->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

        glBindAttribLocation(widget_renderer->text_shader.prog_id, 0, "vertex");
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    return widget_renderer;
}

void
widget_renderer_destroy(te_widget_renderer* widget_renderer) {
    if (widgets_render_data_count(widget_renderer->text_widget_data) > 0) {
        log_error("widget renderer is being destroyed but there are still some text widgets rendering");
        abort();
    }
    widgets_render_data_destroy(widget_renderer->text_widget_data);

    if (widgets_render_data_count(widget_renderer->rect_widget_data) > 0) {
        log_error("widget renderer is being destroyed but there are still some rect widgets rendering");
        abort();
    }
    widgets_render_data_destroy(widget_renderer->rect_widget_data);

    shader_manager_mark_unused_shader(
        renderer_get_shader_manager(widget_renderer->renderer), widget_renderer->text_shader.prog_id);
    shader_manager_mark_unused_shader(
        renderer_get_shader_manager(widget_renderer->renderer), widget_renderer->quad_shader.prog_id);

    glDeleteBuffers(1, &widget_renderer->vbo);
    glDeleteBuffers(1, &widget_renderer->ebo);

    free(widget_renderer);
}

unsigned int
widget_renderer_add_text_widget(te_widget_renderer* renderer) {
    const unsigned int handle = widgets_render_data_add_widget(renderer->text_widget_data);

    // Init data.
    te_text_widget_render_data* data = widgets_render_data_get_widget_data(renderer->text_widget_data, handle);
    memset(data, 0, sizeof(te_text_widget_render_data));

    return handle;
}

te_text_widget_render_data*
widget_renderer_get_text_widget_render_data_tmp(te_widget_renderer* renderer, unsigned int handle) {
    return widgets_render_data_get_widget_data(renderer->text_widget_data, handle);
}

te_rect_widget_render_data*
widget_renderer_get_rect_widget_render_data_tmp(te_widget_renderer* renderer, unsigned int handle) {
    return widgets_render_data_get_widget_data(renderer->rect_widget_data, handle);
}

void
widget_renderer_remove_text_widget(te_widget_renderer* renderer, unsigned int handle) {
    // Cleanup.
    te_text_widget_render_data* data = widgets_render_data_get_widget_data(renderer->text_widget_data, handle);
    free(data->glyphs);

    widgets_render_data_remove_widget(renderer->text_widget_data, handle);
}

unsigned int
widget_renderer_add_rect_widget(te_widget_renderer* renderer) {
    const unsigned int handle = widgets_render_data_add_widget(renderer->rect_widget_data);

    // Init data.
    te_rect_widget_render_data* data = widgets_render_data_get_widget_data(renderer->rect_widget_data, handle);
    memset(data, 0, sizeof(te_rect_widget_render_data));

    return handle;
}

void
widget_renderer_remove_rect_widget(te_widget_renderer* renderer, unsigned int handle) {
    widgets_render_data_remove_widget(renderer->rect_widget_data, handle);
}

void
widget_renderer_draw(te_widget_renderer* widget_renderer) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(renderer_get_window(widget_renderer->renderer), &window_width, &window_height);
    vec2 window_size;
    glm_vec2_copy((vec2){(float)window_width, (float)window_height}, window_size);

    // Draw rect widgets.
    const unsigned int rect_widget_count = widgets_render_data_count(widget_renderer->rect_widget_data);
    if (rect_widget_count > 0) {
        te_quad_shader_data* shader = &widget_renderer->quad_shader;
        GPU_SECTION_BEGIN("rect");

        glUseProgram(shader->prog_id);
        glBindBuffer(GL_ARRAY_BUFFER, widget_renderer->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, widget_renderer->ebo);
        glActiveTexture(GL_TEXTURE0); // quad texture

        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), NULL);

        glUniform2fv(shader->uniform_window_size, 1, window_size);

        for (unsigned int widget_idx = 0; widget_idx < rect_widget_count; widget_idx++) {
            te_rect_widget_render_data* data = widgets_render_data_get(widget_renderer->rect_widget_data);
            data += widget_idx;

            glUniform4fv(shader->uniform_quad_color, 1, data->color);
            glUniform4fv(shader->uniform_clip_rect, 1, data->clip_rect);

            glUniform2fv(shader->uniform_in_pos, 1, data->pos_pix);
            glUniform2fv(shader->uniform_in_size, 1, data->size_pix);

            glUniform1i(shader->uniform_is_using_tex, data->tex_id > 0);
            glBindTexture(GL_TEXTURE_2D, data->tex_id);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
        }

        GPU_SECTION_END;
    }

    // Draw text widgets.
    const unsigned int text_widget_count = widgets_render_data_count(widget_renderer->text_widget_data);
    if (text_widget_count > 0) {
        te_text_shader_data* shader = &widget_renderer->text_shader;
        GPU_SECTION_BEGIN("text");

        glUseProgram(shader->prog_id);
        glBindBuffer(GL_ARRAY_BUFFER, widget_renderer->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, widget_renderer->ebo);
        glActiveTexture(GL_TEXTURE0); // glyph's bitmap

        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), NULL);

        vec4 clip_rect;
        glm_vec4_copy((vec4){0.0f, 0.0f, 1.0f, 1.0f}, clip_rect);
        glUniform4fv(shader->uniform_clip_rect, 1, clip_rect);

        glUniform2fv(shader->uniform_window_size, 1, window_size);

        for (unsigned int widget_idx = 0; widget_idx < text_widget_count; widget_idx++) {
            te_text_widget_render_data* data = widgets_render_data_get(widget_renderer->text_widget_data);
            data += widget_idx;

            glUniform4fv(shader->uniform_text_color, 1, data->color);

            vec2 pos_pix;
            for (unsigned int glyph_idx = 0; glyph_idx < data->glyph_count; glyph_idx++) {
                glm_vec2_add(data->pos_pix, data->glyphs[glyph_idx].offset_pix, pos_pix);

                glUniform2fv(shader->uniform_in_pos, 1, pos_pix);
                glUniform2fv(shader->uniform_in_size, 1, data->glyphs[glyph_idx].size_pix);

                glBindTexture(GL_TEXTURE_2D, data->glyphs[glyph_idx].tex_id);

                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
            }
        }

        GPU_SECTION_END;
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
