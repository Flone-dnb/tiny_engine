#pragma once

#include "cglm/ivec3.h"
#include "cglm/mat3.h"
#include "cglm/mat4.h"
#include "cglm/vec2.h"
#include "cglm/vec4.h"
#include "shape/aabb_shape.h"

typedef struct te_model_renderer te_model_renderer;

struct te_frustum_shape;

// Data used to submit a model for rendering.
typedef struct te_model_render_data {
    mat4 world_mat;
    mat3 normal_mat;
    vec4 color;

    // Texture tiling multiplier. Must store -1 if @ref tex_id is 0.
    vec2 tex_tiling;

    // 0 if not used.
    unsigned int tex_id;

    // 128 bytes ---------

    te_aabb_shape aabb_world;

    vec2 uv_offset;

    // Number of elements in the index buffer.
    int index_count;

    unsigned int vbo;
    unsigned int ebo;
} te_model_render_data;

te_model_renderer* model_renderer_create();
void model_renderer_destroy(te_model_renderer* renderer);

// Adds a new model to be rendered.
// Returns handle to update model's render data using @ref model_renderer_get_render_data_tmp.
//
// Later you would need to remove the model from rendering using @ref model_renderer_remove_model.
unsigned int model_renderer_add_model(te_model_renderer* renderer, unsigned int shader_prog_id);

// Removes a model from rendering.
void model_renderer_remove_model(te_model_renderer* renderer, unsigned int handle);

// Returns pointer to a model's render data to update/modify.
//
// Never store/save this pointer because on the next frame
// the pointer may end up pointing to an invalid memory. Only use this function to quickly
// update some render data. Suffix "_tmp" is used because of this.
//
// Do not free/destroy returned pointer.
te_model_render_data* model_renderer_get_render_data_tmp(te_model_renderer* renderer, unsigned int handle);

// Draws models to the currently set framebuffer.
void model_renderer_draw(te_model_renderer* renderer, ivec4* gl_viewport, mat4* view_proj_mat,
                         struct te_frustum_shape* camera_frustum);
