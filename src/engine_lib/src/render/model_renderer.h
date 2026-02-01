#pragma once

#include "cglm/cglm.h"

typedef struct te_model_renderer te_model_renderer;

/** Data used to render a model. */
typedef struct te_model_render_data {
    /** World matrix. */
    mat4 world_mat;

    /** Normal matrix. */
    mat3 normal_mat;

    /** Color of the model. */
    vec4 color;

    /** Texture tiling multiplier. Must store -1 if @ref tex_id is 0. */
    vec2 tex_tiling;

    /** 0 if not used. */
    unsigned int tex_id;

    // 128 bytes ---------

    /** Offset for UV coordinates. */
    vec2 uv_offset;

    /** Number of elements in the index buffer. */
    int index_count;

    /** Vertex buffer object ID. */
    unsigned int vbo;

    /** Element buffer object ID. */
    unsigned int ebo;
} te_model_render_data;

/**
 * Creates model renderer.
 *
 * @return Created model renderer.
 */
te_model_renderer* model_renderer_create();

/**
 * Destroys model renderer.
 *
 * @param renderer Renderer to destroy.
 */
void model_renderer_destroy(te_model_renderer* renderer);

/**
 * Adds a new model to be rendered.
 *
 * @remark Later you would need to remove the model from rendering using @ref model_renderer_remove_model.
 *
 * @param renderer Renderer.
 * @param prog_id  OpenGL of the shader program the model is using.
 *
 * @return Handle to update model's render data using @ref model_renderer_get_render_data_tmp.
 */
unsigned int model_renderer_add_model(te_model_renderer* renderer, unsigned int prog_id);

/**
 * Removes a model from rendering.
 *
 * @param renderer Renderer.
 * @param handle Handle received from @ref model_renderer_add_model.
 */
void model_renderer_remove_model(te_model_renderer* renderer, unsigned int handle);

/**
 * Returns pointer to a model's render data to update/modify.
 *
 * @warning Never store/save this pointer because on the next frame
 * the pointer may end up pointing to an invalid memory. Only use this function to quickly
 * update some render data. Suffix "_tmp" is used because of this.
 *
 * @param renderer Renderer.
 * @param handle   Model's render data handle.
 *
 * @return Render data. Do not free/destroy returned pointer.
 */
te_model_render_data* model_renderer_get_render_data_tmp(te_model_renderer* renderer, unsigned int handle);

/**
 * Draws models to the currently set framebuffer.
 *
 * @param renderer      Renderer.
 * @param gl_viewport   OpenGL viewport rectangle (in pixels).
 * @param view_proj_mat View projection matrix.
 */
void model_renderer_draw(te_model_renderer* renderer, ivec4 gl_viewport, mat4 view_proj_mat);
