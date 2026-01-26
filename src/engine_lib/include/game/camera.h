#pragma once

#include "cglm/cglm.h"

typedef struct te_camera te_camera;

/**
 * Creates a new camera.
 *
 * @return Created camera.
 */
te_camera* camera_create();

/**
 * Destroys a camera.
 *
 * @param camera Camera to destroy.
 */
void camera_destroy(te_camera* camera);

/**
 * Sets location of the camera.
 *
 * @param camera   Camera.
 * @param location New location of the camera.
 */
void camera_set_location(te_camera* camera, vec3 location);

/**
 * Sets rotation (in degrees) of the camera.
 *
 * @param camera   Camera.
 * @param rotation New rotation of the camera.
 */
void camera_set_rotation(te_camera* camera, vec3 rotation);

/**
 * Sets camera's vertical field of view (in degrees).
 *
 * @param camera       Camera.
 * @param vertical_fov Vertical FOV.
 */
void camera_set_vertical_fov(te_camera* camera, unsigned char vertical_fov);

/**
 * Sets distance to camera's near clip plane.
 *
 * @param camera    Camera.
 * @param near_clip Non-negative value.
 */
void camera_set_near_clip(te_camera* camera, float near_clip);

/**
 * Sets distance to camera's far clip plane.
 *
 * @param camera   Camera.
 * @param far_clip Non-negative value.
 */
void camera_set_far_clip(te_camera* camera, float far_clip);

/**
 * Sets camera's viewport rectangle.
 * Position of the top-left corner of the viewport rectangle in XY and size in ZW (in range [0; 1]).
 *
 * @param camera   Camera.
 * @param viewport Viewport rectangle.
 */
void camera_set_viewport(te_camera* camera, vec4 viewport);

/**
 * Returns location of the camera.
 *
 * @param camera Camera.
 * @param out    Value to write the location to.
 *
 * @return Location.
 */
void camera_get_location(te_camera* camera, vec3 out);

/**
 * Returns rotation of the camera (in degrees).
 *
 * @param camera Camera.
 * @param out    Value to write the rotation to.
 *
 * @return Rotation in degrees.
 */
void camera_get_rotation(te_camera* camera, vec3 out);

/**
 * Returns forward direction of the camera.
 *
 * @param camera Camera.
 * @param out    Value to write the result to.
 */
void camera_get_forward(te_camera* camera, vec3 out);

/**
 * Returns right direction of the camera.
 *
 * @param camera Camera.
 * @param out    Value to write the result to.
 */
void camera_get_right(te_camera* camera, vec3 out);

/**
 * Returns up direction of the camera.
 *
 * @param camera Camera.
 * @param out    Value to write the result to.
 */
void camera_get_up(te_camera* camera, vec3 out);

/**
 * Returns camera's vertical field of view (in degrees).
 *
 * @param camera Camera.
 *
 * @return Vertical FOV.
 */
unsigned char camera_get_vertical_fov(te_camera* camera);

/**
 * Returns distance to camera's near clip plane.
 *
 * @param camera Camera.
 *
 * @return Near Z distance.
 */
float camera_get_near_clip(te_camera* camera);

/**
 * Returns distance to camera's far clip plane.
 *
 * @param camera Camera.
 *
 * @return Far Z distance.
 */
float camera_get_far_clip(te_camera* camera);

/**
 * Returns camera's viewport rectangle.
 * Position of the top-left corner of the viewport rectangle in XY and size in ZW (in range [0; 1]).
 *
 * @param camera Camera.
 * @param out    Viewport rectangle.
 */
void camera_get_viewport(te_camera* camera, vec4 out);

/**
 * Returns camara's view matrix.
 *
 * @param camera Camera.
 * @param out    Value to set the view matrix to.
 */
void camera_get_view_mat(te_camera* camera, mat4 out);

/**
 * Returns camera's projection matrix.
 *
 * @param camera Camera.
 * @param out    Value to set the projection matrix to.
 */
void camera_get_proj_mat(te_camera* camera, mat4 out);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

/**
 * Sets size of the render target (used to calculate aspect ratio for the projection matrix).
 * Does nothing if the specified render target size is already set to the same value.
 *
 * @param camera Camera.
 * @param width  Width (in pixels).
 * @param height Height (in pixels).
 */
void prv_camera_set_render_target_size(te_camera* camera, unsigned int width, unsigned int height);
