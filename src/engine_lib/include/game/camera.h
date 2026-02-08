#pragma once

#include "cglm/mat4.h"
#include "cglm/vec3.h"
#include "cglm/vec4.h"

typedef struct te_camera te_camera;

te_camera* camera_create();
void camera_destroy(te_camera* camera);

// Sets location of the camera.
void camera_set_location(te_camera* camera, vec3 location);

// Sets rotation (in degrees) of the camera.
void camera_set_rotation(te_camera* camera, vec3 rotation);

// Sets camera's vertical field of view (in degrees).
void camera_set_vertical_fov(te_camera* camera, unsigned char vertical_fov);

// Sets distance to camera's near clip plane.
void camera_set_near_clip(te_camera* camera, float near_clip);

// Sets distance to camera's far clip plane.
void camera_set_far_clip(te_camera* camera, float far_clip);

// Sets camera's viewport rectangle.
// Position of the top-left corner of the viewport rectangle in XY and size in ZW (in range [0; 1]).
void camera_set_viewport(te_camera* camera, vec4 viewport);

// Returns location of the camera.
void camera_get_location(te_camera* camera, vec3 out);

// Returns rotation of the camera (in degrees).
void camera_get_rotation(te_camera* camera, vec3 out);

// Returns "forward" direction of the camera.
void camera_get_forward(te_camera* camera, vec3 out);

// Returns "right" direction of the camera.
void camera_get_right(te_camera* camera, vec3 out);

// Returns "up" direction of the camera.
void camera_get_up(te_camera* camera, vec3 out);

// Returns camera's vertical field of view (in degrees).
unsigned char camera_get_vertical_fov(te_camera* camera);

// Returns distance to camera's near clip plane.
float camera_get_near_clip(te_camera* camera);

// Returns distance to camera's far clip plane.
float camera_get_far_clip(te_camera* camera);

// Returns camera's viewport rectangle.
// Position of the top-left corner of the viewport rectangle in XY and size in ZW (in range [0; 1]).
void camera_get_viewport(te_camera* camera, vec4 out);

// Returns camara's view matrix.
void camera_get_view_mat(te_camera* camera, mat4 out);

// Returns camera's projection matrix.
void camera_get_proj_mat(te_camera* camera, mat4 out);

// Returns NULL if the camera is not spawned in a world.
struct te_world* camera_get_world(te_camera* camera);

// Always valid pointer. Do not free/destroy returned pointer. Valid while the camera exists.
struct te_frustum_shape* camera_get_frustum(te_camera* camera);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Sets size (in pixels) of the render target (used to calculate aspect ratio for the projection matrix).
// Does nothing if the specified render target size is already set to the same value.
void prv_camera_set_render_target_size(te_camera* camera, unsigned int width, unsigned int height);

// Sets world the camera is spawned in (specify NULL to mark despawn).
void prv_camera_set_world(te_camera* camera, struct te_world* world);
