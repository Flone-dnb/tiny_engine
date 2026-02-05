#pragma once

#include "cglm/cglm.h"

typedef struct te_model te_model;
struct te_world;

// Creates and loads a new model.
//
// Specify path (relative to the `res` directory) to the file that stores mesh geometry.
// Specify NULL if a default model should be used.
te_model* model_create(const char* path_to_geo);
void model_destroy(te_model* model);

// Sets location of the model.
void model_set_location(te_model* model, vec3 location);

// Sets rotation (in degrees) of the model.
void model_set_rotation(te_model* model, vec3 rotation);

// Sets scale of the model.
void model_set_scale(te_model* model, vec3 scale);

// Sets color of the model in the RGBA format in range [0.0; 1.0].
void model_set_color(te_model* model, vec4 color);

// Sets path (relative to the `res` directory) to texture to use.
// The path string will be copied and stored in the model. Specify NULL to remove texture.
void model_set_texture(te_model* model, const char* relative_path);

// Sets texture tiling multiplier.
void model_set_texture_tiling(te_model* model, vec2 tex_tiling);

// Sets offset for UV coordinates.
void model_set_uv_offset(te_model* model, vec2 uv_offset);

// Sets custom vertex shader.
//
// Can only be used before the model is spawned.
//
// Specify path to the shader file (relative to the `res` directory). The string will be copied
// to the model's object.
void model_set_custom_vert_shader(te_model* model, const char* vert_relative_path);

// Sets custom fragment shader.
//
// Can only be used before the model is spawned.
//
// Specify path to the shader file (relative to the `res` directory). The string will be copied
// to the model's object.
void model_set_custom_frag_shader(te_model* model, const char* frag_relative_path);

// Returns location of the model.
void model_get_location(te_model* model, vec3 out);

// Returns rotation of the model (in degrees).
void model_get_rotation(te_model* model, vec3 out);

// Returns scale of the model.
void model_get_scale(te_model* model, vec3 out);

// Returns RGBA color of the model.
void model_get_color(te_model* model, vec4 out);

// Returns NULL if texture is not set, otherwise path (relative to the `res` directory)
// to the used texture.
// Do not free returned string, valid while the model exists.
const char* model_get_texture(te_model* model);

// Returns texture tiling multiplier.
void model_get_texture_tiling(te_model* model, vec2 tex_tiling);

// Returns offset for UV coordinates.
void model_get_uv_offset(te_model* model, vec2 uv_offset);

// Returns NULL if the model is not spawned.
// Do not free/destroy returned pointer, valid while the model exists.
struct te_world* model_get_world(te_model* model);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Called by world after the model was spawned.
void prv_model_on_spawned(te_model* model, struct te_world* world);

// Called by world before the model is despawned.
void prv_model_on_despawned(te_model* model);

void prv_model_set_vertex_attributes();
