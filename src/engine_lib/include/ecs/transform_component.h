#pragma once

#include "cglm/cglm.h"

/** Allows entities to have a specific location, rotation and size in the world. */
struct te_transform_component;

/**
 * Sets location.
 *
 * @param comp Component data.
 * @param in   Value to set.
 */
void transform_set_location(struct te_transform_component* comp, vec3 in);

/**
 * Sets rotation in degrees.
 *
 * @param comp Component data.
 * @param in   Value to set.
 */
void transform_set_rotation(struct te_transform_component* comp, vec3 in);

/**
 * Sets scale.
 *
 * @param comp Component data.
 * @param in   Value to set.
 */
void transform_set_scale(struct te_transform_component* comp, vec3 in);

/**
 * Returns location.
 *
 * @param comp Component data.
 * @param out  Value to write the result to.
 */
void transform_get_location(struct te_transform_component* comp, vec3 out);

/**
 * Returns rotation in radians.
 *
 * @param comp Component data.
 * @param out  Value to write the result to.
 */
void transform_get_rotation(struct te_transform_component* comp, vec3 out);

/**
 * Returns scale.
 *
 * @param comp Component data.
 * @param out  Value to write the result to.
 */
void transform_get_scale(struct te_transform_component* comp, vec3 out);

/**
 * Returns normalized forward direction.
 *
 * @param comp Component data.
 * @param out  Value to write the result to. 4th component is zero.
 */
void transform_get_forward(struct te_transform_component* comp, vec4 out);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

/**
 * Returns sizeof(te_transform_component).
 *
 * @return Size in bytes.
 */
unsigned int prv_transform_component_get_sizeof(void);

/**
 * Initializes allocated memory.
 *
 * @param data Allocated component data.
 */
void prv_transform_component_init(void* data);
