#pragma once

#include <stdbool.h>
#include "cglm/cglm.h"

/** Manages camera components. */
struct te_camera_system;

/**
 * Sets ID of an entity (with a camera component) that should be the active camera
 * (the camera used to render the world).
 *
 * @param system    Camera system.
 * @param entity_id ID of an entity to be the active camera.
 */
void camera_system_set_active_camera(struct te_camera_system* system, unsigned int entity_id);

/**
 * Returns `true` if there is an active camera to render the world.
 *
 * @param system Camera system.
 *
 * @return `true` if have active camera.
 */
bool camera_system_is_camera_set(struct te_camera_system* system);

/**
 * Returns active camera's view matrix.
 *
 * @param system Camera system.
 * @param out    Output.
 */
void camera_system_get_camera_view(struct te_camera_system* system, mat4 out);

/**
 * Returns active camera's projection matrix.
 *
 * @param system Camera system.
 * @param out    Output.
 */
void camera_system_get_camera_proj(struct te_camera_system* system, mat4 out);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

struct te_ecs;

/**
 * Creates a new camera system to manage entities with camera components.
 *
 * @param ecs ECS manager. The pointer must be valid while the system exists.
 *
 * @return Camera system.
 */
struct te_camera_system* prv_camera_system_create(struct te_ecs* ecs);

/**
 * Destroys a camera system.
 *
 * @param system System.
 */
void prv_camera_system_destroy(struct te_camera_system* system);
