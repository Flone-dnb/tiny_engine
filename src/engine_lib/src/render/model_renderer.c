#include "model_renderer.h"

#include <string.h>
#include "game/model.h"
#include "glad/glad.h"
#include "misc/error.h"
#include "render/shader_manager.h"

#define INVALID_DATA_INDEX 0xffffffff

// Stores information about models that use the same shader program.
typedef struct te_shader_group {
    // OpenGL ID of the shader program.
    unsigned int prog_id;

    // Number of elements that use the same shader program.
    unsigned int count;

    // uniform locations -----------------------
    int uniform_view_proj_mat;
    int uniform_world_mat;
    int uniform_normal_mat;
    int uniform_color;
    int uniform_tiling;
    int uniform_uv_offset;
} te_shader_group;

struct te_model_renderer {
    // This array stores render data sorted by shader program so first N elements use the same
    // shader program, then next M elements use another shader program and so on. Information
    // about which render data belongs to which shader is stored in @ref shader_groups.
    //
    // Size of this array is @ref render_data_array_size but the actual number of valid
    // (used) elements might be different. When some model's render data is removed all next
    // elements are shifted to the left to make sure the array does not have any "holes".
    // This array does not shrink but the number of used (valid) elements may decrease.
    te_model_render_data* render_data;

    // Stores information about which models use which shaders in @ref render_data.
    // First item in this array points to the first item in @ref render_data.
    // Size of this array is @ref shader_group_count.
    te_shader_group* shader_groups;

    // Index into this array using a model's handle to get index into @ref render_data.
    //
    // Public API users store indices into this array so items cannot be reordered/moved.
    // This array CAN have "holes" in it (invalid items). Invalid items store INVALID_DATA_INDEX value.
    // This array does not shrink. Size of this array is @ref handle_to_data_array_size.
    unsigned int* handle_to_data;

    // Number of elements in @ref shader_groups.
    unsigned int shader_group_count;

    // Size of the array @ref render_data.
    unsigned int render_data_array_size;

    // Size of the array @ref handle_to_data.
    unsigned int handle_to_data_array_size;
};

te_model_renderer*
model_renderer_create() {
    te_model_renderer* renderer = malloc(sizeof(te_model_renderer));

    renderer->render_data_array_size = 128;
    renderer->render_data = malloc(sizeof(te_model_render_data) * renderer->render_data_array_size);

    renderer->shader_groups = NULL;
    renderer->shader_group_count = 0;

    renderer->handle_to_data_array_size = renderer->render_data_array_size;
    renderer->handle_to_data = malloc(sizeof(unsigned int) * renderer->handle_to_data_array_size);
    for (unsigned int i = 0; i < renderer->handle_to_data_array_size; i++) {
        renderer->handle_to_data[i] = INVALID_DATA_INDEX;
    }

    return renderer;
}

void
model_renderer_destroy(te_model_renderer* renderer) {
    if (renderer->shader_group_count > 0) {
        show_error_and_abort(
            "model renderer is being destroyed but there are still some models/handles active (not removed)");
    }

    free(renderer->handle_to_data);
    free(renderer->render_data);
    free(renderer);
}

void
model_renderer_init_uniforms(te_shader_group* group) {
    group->uniform_view_proj_mat = get_uniform_location(group->prog_id, "view_proj_mat");
    group->uniform_world_mat = get_uniform_location(group->prog_id, "world_mat");
    group->uniform_normal_mat = get_uniform_location(group->prog_id, "normal_mat");
    group->uniform_color = get_uniform_location(group->prog_id, "color");
    group->uniform_tiling = get_uniform_location(group->prog_id, "tiling");
    group->uniform_uv_offset = get_uniform_location(group->prog_id, "uv_offset");
}

unsigned int
model_renderer_add_model(te_model_renderer* renderer, unsigned int prog_id) {
    // Find unused handle.
    unsigned int handle = 0;
    bool found = false;
    for (unsigned int i = 0; i < renderer->handle_to_data_array_size; i++) {
        if (renderer->handle_to_data[i] != INVALID_DATA_INDEX) {
            continue;
        }
        handle = i;
        found = true;
    }
    if (!found) {
        // Expand handle array.
        const unsigned int expand_size = 128;
        unsigned int* new_handles =
            malloc(sizeof(unsigned int) * (renderer->handle_to_data_array_size + expand_size));
        memcpy(new_handles, renderer->handle_to_data,
               sizeof(unsigned int) * renderer->handle_to_data_array_size);

        free(renderer->handle_to_data);
        renderer->handle_to_data = new_handles;

        for (unsigned int i = renderer->handle_to_data_array_size;
             i < renderer->handle_to_data_array_size + expand_size; i++) {
            renderer->handle_to_data[i] = INVALID_DATA_INDEX;
        }

        handle = renderer->handle_to_data_array_size;
        renderer->handle_to_data_array_size += expand_size;
    }

    // Check if we need to expand render data array.
    unsigned int render_data_count_before = 0;
    for (unsigned int i = 0; i < renderer->shader_group_count; i++) {
        render_data_count_before += renderer->shader_groups[i].count;
    }
    if ((render_data_count_before + 1) > renderer->render_data_array_size) {
        // Expand the array.
        const unsigned int expand_size = 128;
        te_model_render_data* new_data =
            malloc(sizeof(te_model_render_data) * (renderer->render_data_array_size + expand_size));
        memcpy(new_data, renderer->render_data,
               sizeof(te_model_render_data) * renderer->render_data_array_size);

        free(renderer->render_data);
        renderer->render_data = new_data;
        renderer->render_data_array_size += expand_size;
    }

    // Find shader group.
    found = false;
    unsigned int shader_group_index = 0;
    for (unsigned int i = 0; i < renderer->shader_group_count; i++) {
        if (renderer->shader_groups[i].prog_id != prog_id) {
            continue;
        }

        found = true;
        shader_group_index = i;
        break;
    }
    unsigned int data_index = 0;
    if (!found) {
        // Create a new group.
        te_shader_group* new_groups = malloc(sizeof(te_shader_group) * (renderer->shader_group_count + 1));
        memcpy(new_groups, renderer->shader_groups, sizeof(te_shader_group) * renderer->shader_group_count);

        free(renderer->shader_groups);
        renderer->shader_groups = new_groups;
        // don't increment group count yet

        shader_group_index = renderer->shader_group_count;
        te_shader_group* group = &renderer->shader_groups[shader_group_index];

        renderer->shader_group_count += 1;

        // Init group.
        group->prog_id = prog_id;
        group->count = 1;
        model_renderer_init_uniforms(group);

        data_index = render_data_count_before;
    } else {
        for (unsigned int i = 0; i < shader_group_index + 1; i++) {
            data_index += renderer->shader_groups[i].count;
        }

        // Shift some render data to the right to prepare space for new item.
        // We already made sure that the render data array will be able to fit a new item (see above).
        unsigned int copy_count = 0;
        for (unsigned int i = shader_group_index + 1; i < renderer->shader_group_count; i++) {
            copy_count += renderer->shader_groups[i].count;
        }

        // We already made sure that the render data array will be able to fit a new item (see above).
        memmove(renderer->render_data + (data_index + 1), renderer->render_data + data_index,
                sizeof(te_model_render_data) * copy_count);

        // Update group.
        renderer->shader_groups[shader_group_index].count += 1;
    }

    renderer->handle_to_data[handle] = data_index;
    return handle;
}

void
model_renderer_remove_model(te_model_renderer* renderer, unsigned int handle) {
    if (handle >= renderer->handle_to_data_array_size) {
        show_error_and_abort("the specified model render data handle is invalid");
    }

    const unsigned int data_index = renderer->handle_to_data[handle];

    unsigned int render_data_count_before = 0;
    for (unsigned int i = 0; i < renderer->shader_group_count; i++) {
        render_data_count_before += renderer->shader_groups[i].count;
    }

    // Find shader group.
    unsigned int group_index = 0;
    {
        unsigned int start_index = 0;
        bool found = false;
        for (unsigned int i = 0; i < renderer->shader_group_count; i++) {
            if (data_index >= start_index && data_index < (start_index + renderer->shader_groups[i].count)) {
                group_index = i;
                found = true;
                break;
            }
        }
        if (!found) {
            show_error_and_abort("unable to find shader group from the specified handle");
        }
    }

    // Update group.
    if (renderer->shader_groups[group_index].count == 1) {
        // Delete group.
        if (renderer->shader_group_count == 1) {
            renderer->shader_group_count = 0;
            free(renderer->shader_groups);
            renderer->shader_groups = NULL;
        } else {
            te_shader_group* new_groups =
                malloc(sizeof(te_shader_group) * (renderer->shader_group_count - 1));
            memcpy(new_groups, renderer->shader_groups, sizeof(te_shader_group) * group_index);
            memcpy(new_groups + group_index, renderer->shader_groups + (group_index + 1),
                   sizeof(te_shader_group) * (renderer->shader_group_count - group_index - 1));

            free(renderer->shader_groups);
            renderer->shader_groups = new_groups;
            renderer->shader_group_count -= 1;
        }
    } else {
        renderer->shader_groups[group_index].count -= 1;
    }

    // Update render data.
    memmove(renderer->render_data + data_index, renderer->render_data + (data_index + 1),
            sizeof(te_model_render_data) * (render_data_count_before - data_index - 1));

    // Mark handle as unused.
    renderer->handle_to_data[handle] = INVALID_DATA_INDEX;
}

te_model_render_data*
model_renderer_get_render_data_tmp(te_model_renderer* renderer, unsigned int handle) {
    if (CGLM_UNLIKELY(handle >= renderer->handle_to_data_array_size)) {
        show_error_and_abort("the specified model render data handle is invalid");
    }

    return &renderer->render_data[renderer->handle_to_data[handle]];
}

void
model_renderer_draw(te_model_renderer* renderer, ivec4 gl_viewport, mat4 view_proj_mat) {
    glViewport(gl_viewport[0], gl_viewport[1], gl_viewport[2], gl_viewport[3]);

    unsigned int render_data_idx = 0;

    for (unsigned int group_idx = 0; group_idx < renderer->shader_group_count; group_idx++) {
        te_shader_group* group = &renderer->shader_groups[group_idx];

        glUseProgram(group->prog_id);

        glUniformMatrix4fv(group->uniform_view_proj_mat, 1, GL_FALSE, view_proj_mat[0]);

        for (unsigned int unused = 0; unused < group->count; unused++, render_data_idx++) {
            te_model_render_data* data = &renderer->render_data[render_data_idx];

            glUniformMatrix4fv(group->uniform_world_mat, 1, GL_FALSE, data->world_mat[0]);
            glUniformMatrix3fv(group->uniform_normal_mat, 1, GL_FALSE, data->normal_mat[0]);
            glUniform4fv(group->uniform_color, 1, data->color);
            glUniform2fv(group->uniform_tiling, 1, data->tex_tiling);
            glUniform2fv(group->uniform_uv_offset, 1, data->uv_offset);

            glBindTexture(GL_TEXTURE_2D, data->tex_id); // binds 0 if not set

            glBindBuffer(GL_ARRAY_BUFFER, data->vbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data->ebo);

            prv_model_set_vertex_attributes();

            glDrawElements(GL_TRIANGLES, data->index_count, GL_UNSIGNED_SHORT, NULL);
        }
    }
}
