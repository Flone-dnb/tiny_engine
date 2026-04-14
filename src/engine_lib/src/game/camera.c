#include <game/camera.h>

#include <cglm/cam.h>
#include <stdlib.h>
#include <string.h>
#include <game/model.h>
#include <math_funcs.h>
#include <misc/globals.h>
#include <shape/frustum_shape.h>
#include <type_database.h>
#include <world.h>

struct te_camera {
    // May be outdated, see @ref is_view_mat_outdated and @ref is_proj_mat_outdated.
    te_frustum_shape frustum;

    // Not NULL if spawned. Do not free/destroy this pointer.
    struct te_world* world;

    // NULL if not attached.
    te_model* parent_model;

    // NULL if not set.
    char* name;

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

    bool is_serialization_allowed;
};

te_camera*
camera_create() {
    te_camera* camera = malloc(sizeof(te_camera));

    camera->world = NULL;
    camera->name = NULL;
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
    camera->is_serialization_allowed = true;

    return camera;
}

void
camera_destroy(te_camera* camera) {
    free(camera->name);
    free(camera);
}

const char*
camera_get_type_id(void) {
    return "camera";
}

void
camera_set_name(te_camera* camera, const char* name) {
    free(camera->name);
    camera->name = NULL;

    if (name != NULL) {
        const size_t len = strlen(name);
        camera->name = malloc(sizeof(char) * (len + 1));
        memcpy(camera->name, name, sizeof(char) * len);
        camera->name[len] = 0;
    }
}

const char*
camera_get_name(te_camera* camera) {
    return camera->name;
}

static void
camera_despawn(te_world* world, te_camera* camera) {
    if (camera->world != world) {
        log_error("the model is spawned in the different world");
        abort();
    }

    if (camera->parent_model != NULL) {
        model_attach_camera(camera->parent_model, NULL); // make camera to be in the array of root world objects
    }
    world_despawn_camera(camera->world, camera); // despawn root world object
}

void
camera_register_type(void) {
    te_type_info* info = type_info_create(
        camera_get_type_id(), camera_create, camera_destroy, world_spawn_camera,
        camera_despawn, NULL);
    type_info_add_vec3_variable(info, "position", camera_set_position, camera_get_position);
    type_info_add_vec3_variable(info, "rotation", camera_set_rotation, camera_get_rotation);
    type_info_add_uint_variable(
        info, "vertical_fov", camera_set_vertical_fov, camera_get_vertical_fov);
    type_info_add_float_variable(
        info, "near_clip", camera_set_near_clip, camera_get_near_clip);
    type_info_add_float_variable(info, "far_clip", camera_set_far_clip, camera_get_far_clip);
    type_info_add_string_variable(info, "name", camera_set_name, camera_get_name);

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

void camera_get_world_position(te_camera* camera, vec3 out) {
    // Get camera world pos.
    vec4 camera_pos;
    glm_vec3_copy(camera->position, camera_pos);
    if (camera->parent_model != NULL) {
        mat4* world_mat = prv_model_get_world_mat_tmp(camera->parent_model);

        // Ignore scale.
        mat4 world;
        glm_mat4_copy(*world_mat, world);
        math_normalize_safely(world[0]);
        math_normalize_safely(world[1]);
        math_normalize_safely(world[2]);

        glm_mat4_mulv(world, camera_pos, camera_pos);
    }
    camera_pos[3] = 1.0f;

    glm_vec3_copy(camera_pos, out);
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

static void
recalculate_directions(te_camera* camera) {
    mat4 rot_mat;
    math_make_rotation_mat(camera->rotation, rot_mat);

    if (camera->parent_model != NULL) {
        mat4* world_mat = prv_model_get_world_mat_tmp(camera->parent_model);

        // Ignore scale.
        mat4 world;
        glm_mat4_copy(*world_mat, world);
        math_normalize_safely(world[0]);
        math_normalize_safely(world[1]);
        math_normalize_safely(world[2]);

        glm_mat4_mul(world, rot_mat, rot_mat);
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

bool
camera_calc_cursor_world_dir(
    te_camera* camera, vec2 cursor_relative_pos, vec3 out) {
    if (cursor_relative_pos[0] < camera->viewport[0]
        || cursor_relative_pos[1] < camera->viewport[1]
        || cursor_relative_pos[0] > camera->viewport[0] + camera->viewport[2]
        || cursor_relative_pos[1] > camera->viewport[1] + camera->viewport[3]) {
        // Outside of the game viewport.
        glm_vec3_zero(out);
        return false;
    }

    // Remap to viewport.
    glm_vec2_sub(cursor_relative_pos, camera->viewport, cursor_relative_pos);
    glm_vec2_div(cursor_relative_pos, &camera->viewport[2], cursor_relative_pos);

    // Convert mouse pos to NDC [-1; 1] space.
    vec2 ndc;
    glm_vec2_mul(cursor_relative_pos, (vec2){2.0f, 2.0f}, ndc);
    ndc[1] = 2.0f - ndc[1]; // flip Y
    glm_vec2_sub(ndc, (vec2){1.0f, 1.0f}, ndc);

    // Construct a point in clip space.
    vec4 camera_ray;
    camera_ray[0] = ndc[0];
    camera_ray[1] = ndc[1];
    camera_ray[2] = -1.0f; // forward axis in clip space
    camera_ray[3] = 1.0f;

    // Apply inverse view/proj matrix.
    mat4* view_proj_mat = camera_get_view_proj_mat(camera);
    mat4 inv_view_proj_mat;
    glm_mat4_inv(*view_proj_mat, inv_view_proj_mat);
    glm_mat4_mulv(inv_view_proj_mat, camera_ray, camera_ray);
    glm_vec3_divs(camera_ray, camera_ray[3], camera_ray);

    vec3 camera_pos;
    camera_get_world_position(camera, camera_pos);

    // Get direction from camera pos.
    glm_vec3_sub(camera_ray, camera_pos, camera_ray);
    glm_vec3_normalize(camera_ray);

    glm_vec3_copy(camera_ray, out);

    return true;
}

void
camera_set_is_serialization_allowed(te_camera* camera, bool enable) {
    camera->is_serialization_allowed = enable;
}

bool
camera_is_serialization_allowed(te_camera* camera) {
    return camera->is_serialization_allowed;
}

void
prv_camera_recalc_frustum(te_camera* camera) {
#if defined(DEBUG)
    if (camera->is_directions_outdated) {
        log_error("expected directions to be up to date to recalculate camera's frustum");
        abort();
    }
#endif
    camera->frustum = frustum_shape_create(
        camera->position, camera->forward, camera->up, camera->near_clip, camera->far_clip,
        camera->vertical_fov, (float)camera->render_width / (float)camera->render_height);
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
            log_error("expected render target width/height to be set at this point");
            abort();
        }
#endif
        if (camera->is_proj_mat_outdated) {
            glm_perspective_rh_no(
                glm_rad(camera->vertical_fov),
                (float)camera->render_width / (float)camera->render_height, camera->near_clip,
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
        log_error("expected render target width/height to be set at this point");
        abort();
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
                glm_rad(camera->vertical_fov),
                (float)camera->render_width / (float)camera->render_height, camera->near_clip,
                camera->far_clip, camera->proj_mat);

            camera->is_proj_mat_outdated = false;
        }

        prv_camera_recalc_frustum(camera);
    }

    return &camera->frustum;
}

struct te_model*
camera_get_parent_model(te_camera* camera) {
    return camera->parent_model;
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
