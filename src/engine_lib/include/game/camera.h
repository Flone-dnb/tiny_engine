#pragma once

#include <stdbool.h>
#include <cglm/mat4.h>
#include <math/vec3.h>
#include <math/vec4.h>

typedef struct te_camera te_camera;
struct te_model;
struct te_game_object_info;

te_camera* camera_create(void);
void camera_destroy(te_camera* camera);

/* returns game object info */
struct te_game_object_info* camera_get_game_object_info(void);

/* optionally you can set a name of the camera, the string will be copied
 * returns NULL if was not set previously */
void camera_set_name(te_camera* camera, const char* name);
const char* camera_get_name(te_camera* camera);

/* sets position of the camera
 * returns relative position if the camera has a parent */
void camera_set_position(te_camera* camera, te_vec3 position);
void camera_get_position(te_camera* camera, te_vec3 out);

/* unlike @ref camera_get_position this function considers parent model (if it was set) */
void camera_get_world_position(te_camera* camera, te_vec3 out);

/* sets rotation (in degrees) of the camera */
void camera_set_rotation(te_camera* camera, te_vec3 rotation);
void camera_get_rotation(te_camera* camera, te_vec3 out);

/* sets camera's vertical field of view (in degrees) */
void camera_set_vertical_fov(te_camera* camera, unsigned int vertical_fov);
unsigned int camera_get_vertical_fov(te_camera* camera);

/* sets distance to camera's near/far clip plane */
void camera_set_near_clip(te_camera* camera, float near_clip);
void camera_set_far_clip(te_camera* camera, float far_clip);
float camera_get_near_clip(te_camera* camera);
float camera_get_far_clip(te_camera* camera);

/* sets camera's viewport rectangle
 * position of the top-left corner of the viewport rectangle in XY and size in ZW (in range [0; 1]) */
void camera_set_viewport(te_camera* camera, te_vec4 viewport);
void camera_get_viewport(te_camera* camera, te_vec4 out);

/* returns direction of the camera in the world */
void camera_get_forward(te_camera* camera, te_vec3 out);
void camera_get_right(te_camera* camera, te_vec3 out);
void camera_get_up(te_camera* camera, te_vec3 out);

/* optionally you can set a custom pointer to be stored in the camera */
void camera_set_custom_ptr(te_camera* camera, void* ptr);
void* camera_get_custom_ptr(te_camera* camera);

/* optionally you can set a custom value which will also be saved/loaded along with the camera */
void camera_set_custom_value(te_camera* camera, unsigned int value);
unsigned int camera_get_custom_value(te_camera* camera);

/* optionally you can set a custom callback that will be called before the camera is destroyed */
void camera_set_custom_on_before_destroyed(
    te_camera* camera, void (*custom_on_before_destroyed)(te_camera*));

/* uses mouse cursor's position in range [0.0; 1.0] (relative to the window)
 * and converts it into a world space direction from the camera along the cursor
 * returns `false` if the cursor is outside of the camera's viewport */
bool camera_calc_cursor_world_dir(te_camera* camera, te_vec2 cursor_relative_pos, te_vec3 out);

/* allows disabling serialization of the camera (enabled by default) */
void camera_set_is_serialization_allowed(te_camera* camera, bool enable);
bool camera_is_serialization_allowed(te_camera* camera);

/* returns camera's view projection matrix
 * do not free/destroy returned pointer, valid while the camera exists */
mat4* camera_get_view_proj_mat(te_camera* camera);
mat4* camera_get_view_mat(te_camera* camera);
mat4* camera_get_proj_mat(te_camera* camera);

/* returns NULL if the camera is not spawned in a world */
struct te_world* camera_get_world(te_camera* camera);

/* always valid pointer, do not free/destroy returned pointer, valid while the camera exists */
struct te_frustum_shape* camera_get_frustum(te_camera* camera);

/* returns NULL if not attached to a model */
struct te_model* camera_get_parent_model(te_camera* camera);

/* returns unique ID of this type in the type database */
const char* camera_get_type_id(void);
/* registers the type in the type database */
void camera_register_type(void);

/* ------------------------------------------------------------------------------------------------
 *                                      PRIVATE API
 * ------------------------------------------------------------------------------------------------ */

/* sets size (in pixels) of the render target (used to calculate aspect ratio for the projection matrix)
 * does nothing if the specified render target size is already set to the same value */
void
prv_camera_set_render_target_size(te_camera* camera, unsigned int width, unsigned int height);

/* called by model after attached (parent is NULL if detached) and after model's world matrix changed */
void prv_camera_on_parent_model_world_mat_changed(te_camera* camera, struct te_model* parent);

/* called when the camera became the active camera in a world */
void prv_camera_on_active(te_camera* camera);
void prv_camera_on_deactivated(te_camera* camera);

#if defined(ENGINE_EDITOR)
/* shows or hides a model used to visualize camera in the editor */
void prv_camera_set_editor_shape_visibility(te_camera* camera, bool is_visible);
bool prv_camera_is_editor_shape_visible(te_camera* camera);
#endif
