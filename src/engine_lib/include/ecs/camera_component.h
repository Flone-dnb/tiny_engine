#pragma once

#include <stdbool.h>
#include "cglm/cglm.h"

/** Manages camera properties. */
struct te_camera_component;

/**
 * Sets camera's near clip plane distance.
 *
 * @param comp Component.
 * @param z    Non-negative value.
 */
void camera_set_near_clip(struct te_camera_component* comp, float z);

/**
 * Sets camera's far clip plane distance.
 *
 * @param comp Component.
 * @param z Non-negative value.
 */
void camera_set_far_clip(struct te_camera_component* comp, float z);

/**
 * Sets camera's vertical field of view.
 *
 * @param comp Component.
 * @param fov  Vertical FOV.
 */
void camera_set_vertical_fov(struct te_camera_component* comp, unsigned char fov);

/**
 * Returns camera's near clip plane distance.
 *
 * @param comp Component.
 *
 * @return Near clip.
 */
float camera_get_near_clip(struct te_camera_component* comp);

/**
 * Returns camera's far clip plane distance.
 *
 * @param comp Component.
 *
 * @return Far clip.
 */
float camera_get_far_clip(struct te_camera_component* comp);

/**
 * Returns camera's vertical field of view.
 *
 * @return Vertical FOV.
 */
unsigned char camera_get_vertical_fov(struct te_camera_component* comp);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

/**
 * Returns sizeof(te_camera_component).
 *
 * @return Size in bytes.
 */
unsigned int prv_camera_component_get_sizeof(void);

/**
 * Initializes allocated memory.
 *
 * @param data Allocated component data.
 */
void prv_camera_component_init(void* data);

/**
 * Returns `true` if some properties of the camera changed
 * and view and projection matrices need to be recalculated.
 *
 * @remark This function is used by the camera system.
 *
 * @param comp Component.
 *
 * @return `true` if changed.
 */
bool prv_camera_is_properties_changed(struct te_camera_component* comp);

/**
 * Sets `false` to @ref prv_camera_is_properties_changed.
 *
 * @remark This function is used by the camera system.
 *
 * @param comp Component.
 */
void prv_camera_reset_properties_changed_flag(struct te_camera_component* comp);
