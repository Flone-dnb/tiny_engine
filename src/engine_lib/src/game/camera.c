#include "game/camera.h"

#include "cglm/cam.h"
#include "game/model.h"
#include "math_funcs.h"
#include "misc/error.h"
#include "misc/globals.h"
#include "shape/frustum_shape.h"
#include "type_database.h"

struct te_camera {
    // May be outdated, see @ref is_view_mat_outdated and @ref is_proj_mat_outdated.
    te_frustum_shape frustum;

    // Not NULL if spawned. Do not free/destroy this pointer.
    struct te_world* world;

    // NULL if not attached.
    te_model* parent_model;

    // View matrix. May be outdated, see @ref is_view_mat_outdated.
    mat4 view_mat;

    // Projection matrix. May be outdated, see @ref is_proj_mat_outdated.
    mat4 proj_mat;

    // @ref view_mat and @ref proj_mat multiplied. May be outdated.
    mat4 view_proj_mat;

    // Position of the top-left corner of the viewport rectangle in XY and size in ZW (in range [0; 1]).
    vec4 viewport;

    vec3 position;

    // Rotation in degrees.
    vec3 rotation;

    // Camera's forward direction. May be outdated, see @ref is_directions_outdated.
    vec3 forward;

    // Camera's right direction. May be outdated, see @ref is_directions_outdated.
    vec3 right;

    // Camera's up direction. May be outdated, see @ref is_directions_outdated.
    vec3 up;

    float near_clip;
    float far_clip;

    // Render target size (in pixels).
    unsigned int render_width;
    unsigned int render_height;

    // Vertical FOV in degrees.
    unsigned char vertical_fov;

    // `true` @ref view_mat contains outdated value and needs to be recalculated.
    bool is_view_mat_outdated;

    // `true` if forward, right and up directions are outdated and need to be recalculated.
    bool is_directions_outdated;

    // `true` if @ref proj_mat contains outdated value and needs to be recalculated.
    bool is_proj_mat_outdated;
};

te_camera*
camera_create() {
    te_camera* camera = malloc(sizeof(te_camera));

    camera->world = NULL;
    camera->parent_model = NULL;
    glm_vec3_zero(camera->position);
    glm_vec3_zero(camera->rotation);
    glm_vec3_zero(camera->forward);
    glm_vec3_zero(camera->right);
    glm_vec3_zero(camera->up);
    camera->viewport[0] = 0.0f;
    camera->viewport[1] = 0.0f;
    camera->viewport[2] = 1.0f;
    camera->viewport[3] = 1.0f;
    camera->near_clip = 0.2f;
    camera->far_clip = 150.0f;
    camera->vertical_fov = 90;
    camera->render_width = 0;  // not set yet
    camera->render_height = 0; // not set yet
    camera->is_view_mat_outdated = true;
    camera->is_proj_mat_outdated = true;
    camera->is_directions_outdated = true;

    return camera;
}

void
camera_destroy(te_camera* camera) {
    free(camera);
}

const char*
camera_get_type_id(void) {
    return "camera";
}

void
camera_register_type(void) {
    te_type_info* info = type_info_create(camera_get_type_id());
    type_info_add_vec3_variable(info, "position", camera_set_position, camera_get_position);
    type_info_add_vec3_variable(info, "rotation", camera_set_rotation, camera_get_rotation);
    type_info_add_uint_variable(info, "vertical_fov", camera_set_vertical_fov, camera_get_vertical_fov);
    type_info_add_float_variable(info, "near_clip", camera_set_near_clip, camera_get_near_clip);
    type_info_add_float_variable(info, "far_clip", camera_set_far_clip, camera_get_far_clip);

    type_database_register_type(info);
}

void
camera_set_position(te_camera* camera, vec3 position) {
    glm_vec3_copy(position, camera->position);
    camera->is_view_mat_outdated = true;
}

void
camera_set_rotation(te_camera* camera, vec3 rotation) {
    glm_vec3_copy(rotation, camera->rotation);
    camera->is_view_mat_outdated = true;
    camera->is_directions_outdated = true;
}

void
camera_set_vertical_fov(te_camera* camera, unsigned int vertical_fov) {
    camera->vertical_fov = (unsigned char)vertical_fov;
    camera->is_proj_mat_outdated = true;
}

void
camera_set_near_clip(te_camera* camera, float near_clip) {
    camera->near_clip = near_clip;
    camera->is_proj_mat_outdated = true;
}

void
camera_set_far_clip(te_camera* camera, float far_clip) {
    camera->far_clip = glm_max(camera->near_clip + 1.0f, far_clip);
    camera->is_proj_mat_outdated = true;
}

void
camera_set_viewport(te_camera* camera, vec4 viewport) {
    glm_vec4_clamp(viewport, 0.0f, 1.0f);
    glm_vec4_copy(viewport, camera->viewport);
}

void
camera_get_position(te_camera* camera, vec3 out) {
    glm_vec3_copy(camera->position, out);
}

void
camera_get_rotation(te_camera* camera, vec3 out) {
    glm_vec3_copy(camera->rotation, out);
}

unsigned int
camera_get_vertical_fov(te_camera* camera) {
    return camera->vertical_fov;
}

float
camera_get_near_clip(te_camera* camera) {
    return camera->near_clip;
}

float
camera_get_far_clip(te_camera* camera) {
    return camera->far_clip;
}

void
camera_get_viewport(te_camera* camera, vec4 out) {
    glm_vec4_copy(camera->viewport, out);
}

void
recalculate_directions(te_camera* camera) {
    mat4 rot_mat;
    math_make_rotation_mat(camera->rotation, rot_mat);

    if (camera->parent_model != NULL) {
        mat4* world_mat = prv_model_get_world_mat_tmp(camera->parent_model);
        glm_mat4_mul(*world_mat, rot_mat, rot_mat);
    }

    vec4 global_forward;
    globals_get_world_forward(global_forward);
    global_forward[3] = 0.0f;

    vec4 global_right;
    globals_get_world_right(global_right);
    global_right[3] = 0.0f;

    vec4 global_up;
    globals_get_world_up(global_up);
    global_up[3] = 0.0f;

    vec4 forward, right, up;
    glm_mat4_mulv(rot_mat, global_forward, forward);
    glm_mat4_mulv(rot_mat, global_right, right);
    glm_mat4_mulv(rot_mat, global_up, up);

    glm_vec3_copy(forward, camera->forward);
    glm_vec3_copy(right, camera->right);
    glm_vec3_copy(up, camera->up);

    camera->is_directions_outdated = false;
}

void
camera_get_forward(te_camera* camera, vec3 out) {
    if (camera->is_directions_outdated) {
        recalculate_directions(camera);
    }

    glm_vec3_copy(camera->forward, out);
}

void
camera_get_right(te_camera* camera, vec3 out) {
    if (camera->is_directions_outdated) {
        recalculate_directions(camera);
    }

    glm_vec3_copy(camera->right, out);
}

void
camera_get_up(te_camera* camera, vec3 out) {
    if (camera->is_directions_outdated) {
        recalculate_directions(camera);
    }

    glm_vec3_copy(camera->up, out);
}

void
prv_camera_recalc_frustum(te_camera* camera) {
#if defined(DEBUG)
    if (camera->is_directions_outdated) {
        show_error_and_abort("expected directions to be up to date to recalculate camera's frustum");
    }
#endif
    camera->frustum = frustum_shape_create(
        camera->position, camera->forward, camera->up, camera->near_clip, camera->far_clip, camera->vertical_fov,
        (float)camera->render_width / (float)camera->render_height);
}

mat4*
camera_get_view_proj_mat(te_camera* camera) {
    if (camera->is_view_mat_outdated || camera->is_proj_mat_outdated) {
        if (camera->is_view_mat_outdated) {
            vec3 forward;
            vec3 up;
            camera_get_forward(camera, forward);
            camera_get_up(camera, up);

            glm_look_rh(camera->position, forward, up, camera->view_mat);
            camera->is_view_mat_outdated = false;
        }

#if defined(DEBUG)
        if (camera->render_width == 0 || camera->render_height == 0) {
            show_error_and_abort("expected render target width/height to be set at this point");
        }
#endif
        if (camera->is_proj_mat_outdated) {
            glm_perspective_rh_no(
                glm_rad(camera->vertical_fov), (float)camera->render_width / (float)camera->render_height, camera->near_clip,
                camera->far_clip, camera->proj_mat);
            camera->is_proj_mat_outdated = false;
        }

        prv_camera_recalc_frustum(camera);
        glm_mat4_mul(camera->proj_mat, camera->view_mat, camera->view_proj_mat);
    }

    return &camera->view_proj_mat;
}

struct te_world*
camera_get_world(te_camera* camera) {
    return camera->world;
}

struct te_frustum_shape*
camera_get_frustum(te_camera* camera) {
#if defined(DEBUG)
    if (camera->render_width == 0 || camera->render_height == 0) {
        show_error_and_abort("expected render target width/height to be set at this point");
    }
#endif

    if (camera->is_view_mat_outdated || camera->is_proj_mat_outdated) {
        if (camera->is_view_mat_outdated) {
            vec3 forward;
            vec3 up;
            camera_get_forward(camera, forward);
            camera_get_up(camera, up);

            glm_look_rh(camera->position, forward, up, camera->view_mat);

            camera->is_view_mat_outdated = false;
        }

        if (camera->is_proj_mat_outdated) {
            glm_perspective_rh_no(
                glm_rad(camera->vertical_fov), (float)camera->render_width / (float)camera->render_height, camera->near_clip,
                camera->far_clip, camera->proj_mat);

            camera->is_proj_mat_outdated = false;
        }

        prv_camera_recalc_frustum(camera);
    }

    return &camera->frustum;
}

void
prv_camera_set_render_target_size(te_camera* camera, unsigned int width, unsigned int height) {
    if ((width == camera->render_width) && (height == camera->render_height)) {
        return;
    }

    camera->render_width = width;
    camera->render_height = height;
    camera->is_proj_mat_outdated = true;
}

void
prv_camera_set_world(te_camera* camera, struct te_world* world) {
    camera->world = world;
}

void
prv_camera_on_parent_model_world_mat_changed(te_camera* camera, te_model* parent) {
    camera->is_directions_outdated = true;
    camera->is_view_mat_outdated = true;

    camera->parent_model = parent;
}
