#include <render/model_renderer.h>

#include <string.h>
#include <debug_console.h>
#include <game/model.h>
#include <io/log.h>
#include <render/shader_manager.h>
#include <shape/frustum_shape.h>
#include <glad/glad.h>

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
    int uniform_skin_mats; // -1 if not using skinning
} te_shader_group;

struct te_model_renderer {
    // This array stores render data sorted by shader program so first N elements use the same
    // shader program, then next M elements use another shader program and so on. Information
    // about which render data belongs to which shader is stored in @ref shader_groups.
    //
    // Size of this array is @ref render_handle_arrays_size but the actual number of valid
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
    // This array does not shrink. Size of this array is @ref render_handle_arrays_size.
    unsigned int* handle_to_data;

    // Number of elements in @ref shader_groups.
    unsigned int shader_group_count;

    // Total number of elements that @ref render_data and @ref handle_to_data can store.
    unsigned int render_handle_arrays_size;

    // Determines how much to expand data arrays if reached the limit of @ref render_handle_arrays_size.
    unsigned int expand_size;
};

te_model_renderer*
model_renderer_create(unsigned int capacity, unsigned int expand_size) {
    te_model_renderer* renderer = malloc(sizeof(te_model_renderer));

    renderer->expand_size = expand_size;
    renderer->render_handle_arrays_size = capacity;
    renderer->render_data = malloc(sizeof(te_model_render_data) * renderer->render_handle_arrays_size);

    renderer->handle_to_data = malloc(sizeof(unsigned int) * renderer->render_handle_arrays_size);
    for (unsigned int i = 0; i < renderer->render_handle_arrays_size; i++) {
        renderer->handle_to_data[i] = INVALID_DATA_INDEX;
    }

    renderer->shader_groups = NULL;
    renderer->shader_group_count = 0;

    return renderer;
}

void
model_renderer_destroy(te_model_renderer* renderer) {
    if (renderer->shader_group_count > 0) {
        log_error("model renderer is being destroyed but there are still some models/handles active (not removed)");
        abort();
    }

    free(renderer->handle_to_data);
    free(renderer->render_data);
    free(renderer);
}

bool
model_renderer_has_models(te_model_renderer* renderer) {
    return renderer->shader_group_count > 0;
}

void
model_renderer_init_uniforms(te_shader_group* group) {
    group->uniform_view_proj_mat = get_uniform_location(group->prog_id, "view_proj_mat");
    group->uniform_world_mat = get_uniform_location(group->prog_id, "world_mat");
    group->uniform_normal_mat = get_uniform_location(group->prog_id, "normal_mat");
    group->uniform_color = get_uniform_location(group->prog_id, "color");
    group->uniform_tiling = get_uniform_location(group->prog_id, "tiling");
    group->uniform_uv_offset = get_uniform_location(group->prog_id, "uv_offset");
    group->uniform_skin_mats = -1;
}

unsigned int
model_renderer_add_model(te_model_renderer* renderer, unsigned int prog_id) {
    // Find unused handle.
    unsigned int handle = 0;
    bool found = false;
    for (unsigned int i = 0; i < renderer->render_handle_arrays_size; i++) {
        if (renderer->handle_to_data[i] != INVALID_DATA_INDEX) {
            continue;
        }
        handle = i;
        found = true;
        break;
    }
    if (!found) {
        // Expand handle array.
        unsigned int* new_handles = malloc(sizeof(unsigned int) * (renderer->render_handle_arrays_size + renderer->expand_size));
        memcpy(new_handles, renderer->handle_to_data, sizeof(unsigned int) * renderer->render_handle_arrays_size);

        free(renderer->handle_to_data);
        renderer->handle_to_data = new_handles;

        for (unsigned int i = renderer->render_handle_arrays_size;
             i < renderer->render_handle_arrays_size + renderer->expand_size; i++) {
            renderer->handle_to_data[i] = INVALID_DATA_INDEX;
        }
        handle = renderer->render_handle_arrays_size;

        // Expand render data array.
        te_model_render_data* new_data =
            malloc(sizeof(te_model_render_data) * (renderer->render_handle_arrays_size + renderer->expand_size));
        memcpy(new_data, renderer->render_data, sizeof(te_model_render_data) * renderer->render_handle_arrays_size);

        free(renderer->render_data);
        renderer->render_data = new_data;

        renderer->render_handle_arrays_size += renderer->expand_size;
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
        unsigned int render_data_count_before = 0;
        for (unsigned int i = 0; i < renderer->shader_group_count; i++) {
            render_data_count_before += renderer->shader_groups[i].count;
        }

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
        memmove(
            renderer->render_data + (data_index + 1), renderer->render_data + data_index,
            sizeof(te_model_render_data) * copy_count);

        // Update group.
        renderer->shader_groups[shader_group_index].count += 1;
    }

    renderer->handle_to_data[handle] = data_index;
    return handle;
}

void
model_renderer_remove_model(te_model_renderer* renderer, unsigned int handle) {
    if (handle >= renderer->render_handle_arrays_size) {
        log_error("the specified model render data handle is invalid");
        abort();
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
            log_error("unable to find shader group from the specified handle");
            abort();
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
            te_shader_group* new_groups = malloc(sizeof(te_shader_group) * (renderer->shader_group_count - 1));
            memcpy(new_groups, renderer->shader_groups, sizeof(te_shader_group) * group_index);
            memcpy(
                new_groups + group_index, renderer->shader_groups + (group_index + 1),
                sizeof(te_shader_group) * (renderer->shader_group_count - group_index - 1));

            free(renderer->shader_groups);
            renderer->shader_groups = new_groups;
            renderer->shader_group_count -= 1;
        }
    } else {
        renderer->shader_groups[group_index].count -= 1;
    }

    // Update render data.
    memmove(
        renderer->render_data + data_index, renderer->render_data + (data_index + 1),
        sizeof(te_model_render_data) * (render_data_count_before - data_index - 1));

    // Mark handle as unused.
    renderer->handle_to_data[handle] = INVALID_DATA_INDEX;

    // Shift render data indices after the removed one.
    for (unsigned int i = 0; i < renderer->render_handle_arrays_size; i++) {
        if (renderer->handle_to_data[i] == INVALID_DATA_INDEX || renderer->handle_to_data[i] < data_index) {
            continue;
        }
        renderer->handle_to_data[i] -= 1;
    }
}

te_model_render_data*
model_renderer_get_render_data_tmp(te_model_renderer* renderer, unsigned int handle) {
    if (CGLM_UNLIKELY(handle >= renderer->render_handle_arrays_size)) {
        log_error("the specified model render data handle is invalid");
        abort();
    }

    return &renderer->render_data[renderer->handle_to_data[handle]];
}

void
model_renderer_draw(te_model_renderer* renderer, mat4* view_proj_mat, te_frustum_shape* camera_frustum) {
#if defined(ENGINE_DEBUG_TOOLS)
    te_debug_stats* debug_stats = prv_debug_console_get_stats();
#endif

    unsigned int render_data_idx = 0;

    for (unsigned int group_idx = 0; group_idx < renderer->shader_group_count; group_idx++) {
        te_shader_group* group = &renderer->shader_groups[group_idx];

        glUseProgram(group->prog_id);

        glUniformMatrix4fv(group->uniform_view_proj_mat, 1, GL_FALSE, (*view_proj_mat)[0]);

        for (unsigned int unused = 0; unused < group->count; unused++, render_data_idx++) {
            te_model_render_data* data = &renderer->render_data[render_data_idx];

            // Frustum culling (don't cull skeletal meshes due to animations).
            if (group->uniform_skin_mats == -1 && !frustum_shape_is_aabb_inside(camera_frustum, &data->aabb_world)) {
                continue;
            }

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
#if defined(ENGINE_DEBUG_TOOLS)
            debug_stats->rendered_model_count += 1;
#endif
        }
    }
}
