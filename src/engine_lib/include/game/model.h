#pragma once

#include "cglm/cglm.h"

typedef struct te_model te_model;
struct te_world;

/**
 * Creates and loads a new model.
 *
 * @param path_to_geo Path (relative to the `res` directory) to the file that stores mesh geometry.
 * NULL if instead a default model should be used.
 *
 * @return Loaded model.
 */
te_model* model_create(const char* path_to_geo);

/**
 * Destroys a model.
 *
 * @param model Model to destroy.
 */
void model_destroy(te_model* model);

/**
 * Sets location of the model.
 *
 * @param model    Model.
 * @param location New location.
 */
void model_set_location(te_model* model, vec3 location);

/**
 * Sets rotation (in degrees) of the model.
 *
 * @param model    Model.
 * @param rotation New rotation.
 */
void model_set_rotation(te_model* model, vec3 rotation);

/**
 * Sets scale of the model.
 *
 * @param model Model.
 * @param scale New scale.
 */
void model_set_scale(te_model* model, vec3 scale);

/**
 * Sets color of the model.
 *
 * @param model Model.
 * @param color RGBA color in range [0.0; 1.0].
 */
void model_set_color(te_model* model, vec4 color);

/**
 * Sets custom vertex shader.
 *
 * @remark Can only be used before the model is spawned.
 *
 * @param model Model.
 * @param vert_relative_path Path to the shader file (relative to the `res` directory). The string will be copied
 * to the model's data.
 */
void model_set_custom_vert_shader(te_model* model, const char* vert_relative_path);

/**
 * Sets custom fragment shader.
 *
 * @remark Can only be used before the model is spawned.
 *
 * @param model Model.
 * @param frag_relative_path Path to the shader file (relative to the `res` directory). The string will be copied
 * to the model's data.
 */
void model_set_custom_frag_shader(te_model* model, const char* frag_relative_path);

/**
 * Returns location of the model.
 *
 * @param model Model.
 * @param out   Value to write the location to.
 *
 * @return Location.
 */
void model_get_location(te_model* model, vec3 out);

/**
 * Returns rotation of the model (in degrees).
 *
 * @param model Model.
 * @param out   Value to write the rotation to.
 *
 * @return Rotation in degrees.
 */
void model_get_rotation(te_model* model, vec3 out);

/**
 * Returns scale of the model.
 *
 * @param model Model.
 * @param out   Value to write the rotation to.
 *
 * @return Scale.
 */
void model_get_scale(te_model* model, vec3 out);

/**
 * Returns color of the model.
 *
 * @param model Model.
 * @param out   Value to write the color (RGBA) to.
 */
void model_get_color(te_model* model, vec4 out);

/**
 * Returns world the model is spawned in.
 *
 * @param model Model.
 *
 * @return NULL if not spawned.
 */
struct te_world* model_get_world(te_model* model);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

/**
 * Called by world after the model was spawned.
 *
 * @param model Model that was spawned.
 * @param world World the model was spawned in.
 */
void prv_model_on_spawned(te_model* model, struct te_world* world);

/**
 * Called by world before the model is despawned.
 *
 * @param model Model that is about to be despawned.
 */
void prv_model_on_despawned(te_model* model);
