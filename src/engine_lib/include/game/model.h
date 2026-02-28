#pragma once

#include "cglm/mat4.h"
#include "cglm/vec2.h"
#include "cglm/vec3.h"

typedef struct te_model te_model;
struct te_world;
struct te_camera;

te_model* model_create();
void model_destroy(te_model* model);

// Specify path (relative to the `res` directory) to the file that stores mesh geometry.
// Specify NULL to use default model instead.
void model_set_geometry(te_model* model, const char* relative_path);
const char* model_get_geometry(te_model* model);

// Optionally you can set a name of the model. The string will be copied.
// Returns NULL if was not set previously.
void model_set_name(te_model* model, const char* name);
const char* model_get_name(te_model* model);

void model_set_position(te_model* model, vec3 position);
void model_set_rotation(te_model* model, vec3 rotation); // in degrees
void model_set_scale(te_model* model, vec3 scale);

void model_get_position(te_model* model, vec3 out);
void model_get_rotation(te_model* model, vec3 out); // in degrees
void model_get_scale(te_model* model, vec3 out);

// Sets color of the model in the RGBA format in range [0.0; 1.0].
// Note that alpha will be ignored if @ref model_enable_transparency is disabled.
void model_set_color(te_model* model, vec4 color);
void model_get_color(te_model* model, vec4 out);

// Sets path (relative to the `res` directory) to texture to use.
// The path string will be copied and stored in the model. Specify NULL to remove texture.
// Note that alpha will be ignored if @ref model_enable_transparency is disabled.
void model_set_texture(te_model* model, const char* relative_path);

// Returns NULL if texture is not set.
// Do not free returned string, valid while the model exists.
const char* model_get_texture(te_model* model);

// Sets texture tiling multiplier.
void model_set_texture_tiling(te_model* model, vec2 tex_tiling);
void model_get_texture_tiling(te_model* model, vec2 tex_tiling);

// Sets offset for UV coordinates.
void model_set_uv_offset(te_model* model, vec2 uv_offset);
void model_get_uv_offset(te_model* model, vec2 uv_offset);

// Child model's location/rotation/scale will then be treated as relative to the parents parameters.
// If the child model is not spawned but the parent is spawned will make the child model spawned.
// When parent is despawned/destroyed will also make the attached child despawn/destroy.
// Specify NULL as parent to detach.
void model_set_parent(te_model* model, te_model* new_parent);

// Same as @ref model_set_parent but attaches a camera. Specify NULL to detach the camera.
void model_attach_camera(te_model* model, struct te_camera* camera);

// Sets custom vertex shader.
//
// Specify path to the shader file (relative to the `res` directory). The string will be copied
// to the model's object. Specify NULL to remove.
void model_set_custom_vert_shader(te_model* model, const char* vert_relative_path);
const char* model_get_custom_vert_shader(te_model* model);

// Sets custom fragment shader.
//
// Specify path to the shader file (relative to the `res` directory). The string will be copied
// to the model's object. Specify NULL to remove.
void model_set_custom_frag_shader(te_model* model, const char* frag_relative_path);
const char* model_get_custom_frag_shader(te_model* model);

// Transparency is disabled by default.
// Note that this option should only be used for semi-transparent 2D planes
// (such as grass planes) because there's no sorting for transparent geometry.
void model_enable_transparency(te_model* model, bool enable);
bool model_is_transparency_enabled(te_model* model);

// Returns NULL if the model is not spawned.
// Do not free/destroy returned pointer, valid while the model exists.
struct te_world* model_get_world(te_model* model);

// Returns unique ID of this type in the type database.
const char* model_get_type_id(void);
// Registers the type in the type database.
void model_register_type(void);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Called by world after the model was spawned.
void prv_model_on_spawned(te_model* model, struct te_world* world);

// Called by world before the model is despawned.
void prv_model_on_despawned(te_model* model);

// Returns model's world matrix (includes parent if has any).
mat4* prv_model_get_world_mat_tmp(te_model* model);

void prv_model_set_vertex_attributes();
