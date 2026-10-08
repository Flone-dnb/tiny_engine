#pragma once

#include <math/mat3.h>
#include <math/mat4.h>
#include <math/vec2.h>
#include <shape/aabb_shape.h>

typedef struct te_model_renderer te_model_renderer;

struct te_frustum_shape;
struct te_light_params;

/* data used to submit a model for rendering */
typedef struct te_model_render_data {
    te_mat4 world_mat;
    te_mat3 normal_mat;
    te_vec4 color;

    /* texture tiling multiplier, must store -1 if @ref tex_id is 0 */
    te_vec2 tex_tiling;

    /* 0 if not used */
    unsigned int tex_id;

    /* 128 bytes --------- */

    te_aabb_shape aabb_world;

    te_vec2 uv_offset;

    /* NULL if not using skinning */
    te_mat4* skinning_mats;
    unsigned int skinning_mats_count;

    int index_count;
#if defined(ENGINE_GLES)
    unsigned int vbo;
    unsigned int ebo;
#else
    unsigned int vao;
#endif
} te_model_render_data;

/* capacity is the initial (reserved) size for data arrays where 1 item is used by 1 model
 * expand size determines how much to expand data arrays if reached capacity */
te_model_renderer* model_renderer_create(unsigned int capacity, unsigned int expand_size);
void model_renderer_destroy(te_model_renderer* renderer);

/* returns `true` if at least 1 model needs to be rendered */
bool model_renderer_has_models(te_model_renderer* renderer);

/* adds a new model to be rendered
 * returns handle to update model's render data using @ref model_renderer_get_render_data_tmp
 *
 * later you would need to remove the model from rendering using @ref model_renderer_remove_model */
unsigned int model_renderer_add_model(
    te_model_renderer* renderer, unsigned int shader_prog_id, bool disable_backface_culling);

/* removes a model from rendering */
void model_renderer_remove_model(te_model_renderer* renderer, unsigned int handle);

/* returns pointer to a model's render data to update/modify
 * 
 * never store/save this pointer because on the next frame
 * the pointer may end up pointing to an invalid memory. Only use this function to quickly
 * update some render data. Suffix "_tmp" is used because of this.
 *
 * do not free/destroy returned pointer */
te_model_render_data*
model_renderer_get_render_data_tmp(te_model_renderer* renderer, unsigned int handle);

/* draws models to the currently set framebuffer
 * returns the number of models drawn */
unsigned int model_renderer_draw(
    te_model_renderer* renderer, struct te_light_params* light_params, te_mat4* view_mat,
    te_mat4* view_proj_mat, struct te_frustum_shape* camera_frustum);
