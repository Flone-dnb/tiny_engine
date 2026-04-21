#pragma once

#include <stdbool.h>
#include <cglm/mat4.h>
#include <cglm/vec3.h>
#include <cglm/vec4.h>

typedef struct te_camera te_camera;
struct te_model;

te_camera* camera_create();
void camera_destroy(te_camera* camera);

// Optionally you can set a name of the camera. The string will be copied.
// Returns NULL if was not set previously.
void camera_set_name(te_camera* camera, const char* name);
const char* camera_get_name(te_camera* camera);

// Sets position of the camera.
// Returns relative position if the camera has a parent.
void camera_set_position(te_camera* camera, vec3 position);
void camera_get_position(te_camera* camera, vec3 out);

// Unlike @ref camera_get_position this function considers parent model (if it was set).
void camera_get_world_position(te_camera* camera, vec3 out);

// Sets rotation (in degrees) of the camera.
void camera_set_rotation(te_camera* camera, vec3 rotation);
void camera_get_rotation(te_camera* camera, vec3 out);

// Sets camera's vertical field of view (in degrees).
void camera_set_vertical_fov(te_camera* camera, unsigned int vertical_fov);
unsigned int camera_get_vertical_fov(te_camera* camera);

// Sets distance to camera's near/far clip plane.
void camera_set_near_clip(te_camera* camera, float near_clip);
void camera_set_far_clip(te_camera* camera, float far_clip);
float camera_get_near_clip(te_camera* camera);
float camera_get_far_clip(te_camera* camera);

// Sets camera's viewport rectangle.
// Position of the top-left corner of the viewport rectangle in XY and size in ZW (in range [0; 1]).
void camera_set_viewport(te_camera* camera, vec4 viewport);
void camera_get_viewport(te_camera* camera, vec4 out);

// Returns direction of the camera in the world.
void camera_get_forward(te_camera* camera, vec3 out);
void camera_get_right(te_camera* camera, vec3 out);
void camera_get_up(te_camera* camera, vec3 out);

// Uses mouse cursor's position in range [0.0; 1.0] (relative to the window)
// and converts it into a world space direction from the camera along the cursor.
// Returns `false` if the cursor is outside of the camera's viewport.
bool camera_calc_cursor_world_dir(te_camera* camera, vec2 cursor_relative_pos, vec3 out);

// Allows disabling serialization of the camera (enabled by default).
void camera_set_is_serialization_allowed(te_camera* camera, bool enable);
bool camera_is_serialization_allowed(te_camera* camera);

// Returns camera's view projection matrix.
// Do not free/destroy returned pointer, valid while the camera exists.
mat4* camera_get_view_proj_mat(te_camera* camera);

// Returns NULL if the camera is not spawned in a world.
struct te_world* camera_get_world(te_camera* camera);

// Always valid pointer. Do not free/destroy returned pointer. Valid while the camera exists.
struct te_frustum_shape* camera_get_frustum(te_camera* camera);

// Returns NULL if not attached to a model.
struct te_model* camera_get_parent_model(te_camera* camera);

// Returns unique ID of this type in the type database.
const char* camera_get_type_id(void);
// Registers the type in the type database.
void camera_register_type(void);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Sets size (in pixels) of the render target (used to calculate aspect ratio for the projection matrix).
// Does nothing if the specified render target size is already set to the same value.
void
prv_camera_set_render_target_size(te_camera* camera, unsigned int width, unsigned int height);

// Sets world the camera is spawned in (specify NULL to mark despawn).
void prv_camera_set_world(te_camera* camera, struct te_world* world);

// Called by model after attached (parent is NULL if detached) and after model's world matrix changed.
void prv_camera_on_parent_model_world_mat_changed(te_camera* camera, struct te_model* parent);

// Called when the camera became the active camera in a world.
void prv_camera_on_active(te_camera* camera);