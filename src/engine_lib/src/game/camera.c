#include "game/camera.h"

#include "math_funcs.h"
#include "misc/error.h"
#include "misc/globals.h"

/** It's a game camera alright. */
struct te_camera {
    /** Not NULL if spawned. Do not free/destroy this pointer. */
    struct te_world* world;

    /** View matrix. May be outdated, see @ref is_view_mat_outdated. */
    mat4 view_mat;

    /** Projection matrix. May be outdated, see @ref is_proj_mat_outdated. */
    mat4 proj_mat;

    /** Position of the top-left corner of the viewport rectangle in XY and size in ZW (in range [0; 1]). */
    vec4 viewport;

    /** Location. */
    vec3 location;

    /** Rotation in degrees. */
    vec3 rotation;

    /** Camera's forward direction. May be outdated, see @ref is_directions_outdated. */
    vec3 forward;

    /** Camera's right direction. May be outdated, see @ref is_directions_outdated. */
    vec3 right;

    /** Camera's up direction. May be outdated, see @ref is_directions_outdated. */
    vec3 up;

    /** Distance to the camera's near clip plane. */
    float near_clip;

    /** Distance to the camera's far clip plane. */
    float far_clip;

    /** Width (in pixels) of the render target. */
    unsigned int render_width;

    /** Height (in pixels) of the render target. */
    unsigned int render_height;

    /** Vertical field of view of the camera (in degrees). */
    unsigned char vertical_fov;

    /** `true` @ref view_mat contains outdated value and needs to be recalculated. */
    bool is_view_mat_outdated;

    /** `true` if forward, right and up directions are outdated and need to be recalculated. */
    bool is_directions_outdated;

    /** `true` if @ref proj_mat contains outdated value and needs to be recalculated. */
    bool is_proj_mat_outdated;
};

te_camera*
camera_create() {
    te_camera* camera = malloc(sizeof(te_camera));

    camera->world = NULL;
    glm_vec3_zero(camera->location);
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

void
camera_set_location(te_camera* camera, vec3 location) {
    glm_vec3_copy(location, camera->location);
    camera->is_view_mat_outdated = true;
}

void
camera_set_rotation(te_camera* camera, vec3 rotation) {
    glm_vec3_copy(rotation, camera->rotation);
    camera->is_view_mat_outdated = true;
    camera->is_directions_outdated = true;
}

void
camera_set_vertical_fov(te_camera* camera, unsigned char vertical_fov) {
    camera->vertical_fov = vertical_fov;
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
camera_get_location(te_camera* camera, vec3 out) {
    glm_vec3_copy(camera->location, out);
}

void
camera_get_rotation(te_camera* camera, vec3 out) {
    glm_vec3_copy(camera->rotation, out);
}

unsigned char
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
camera_get_view_mat(te_camera* camera, mat4 out) {
    if (camera->is_view_mat_outdated) {
        vec3 forward;
        vec3 up;
        camera_get_forward(camera, forward);
        camera_get_up(camera, up);

        glm_look_rh(camera->location, forward, up, camera->view_mat);

        camera->is_view_mat_outdated = false;
    }

    glm_mat4_copy(camera->view_mat, out);
}

void
camera_get_proj_mat(te_camera* camera, mat4 out) {
#if defined(DEBUG)
    if (camera->render_width == 0 || camera->render_height == 0) {
        show_error_and_abort("expected render target width/height to be set at this point");
    }
#endif

    if (camera->is_proj_mat_outdated) {
        glm_perspective_rh_no(glm_rad(camera->vertical_fov),
                              (float)camera->render_width / (float)camera->render_height, camera->near_clip,
                              camera->far_clip, camera->proj_mat);

        camera->is_proj_mat_outdated = false;
    }

    glm_mat4_copy(camera->proj_mat, out);
}

struct te_world*
camera_get_world(te_camera* camera) {
    return camera->world;
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
