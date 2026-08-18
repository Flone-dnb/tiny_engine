#include <game/camera.h>

#include <cglm/cam.h>
#include <stdlib.h>
#include <string.h>
#include <game/model.h>
#include <game/game_object_info.h>
#include <math_funcs.h>
#include <misc/globals.h>
#include <shape/frustum_shape.h>
#include <type_database.h>
#include <misc/mesh_generator.h>
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

    // NULL if not set. Custom callback.
    void (*custom_on_before_destroyed)(te_camera*);
    void* custom_ptr;
    unsigned int custom_value;

#if defined(ENGINE_EDITOR)
    // Model to visualize the camera in the editor.
    te_model* editor_model;
    bool is_editor_model_visible;
#endif

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

static void on_spawned(te_camera* camera, struct te_world* world);
static void on_despawned(te_camera* camera);

te_camera*
camera_create() {
    te_camera* camera = malloc(sizeof(te_camera));

    camera->world = NULL;
    camera->name = NULL;
    camera->parent_model = NULL;
#if defined(ENGINE_EDITOR)
    camera->editor_model = NULL;
    camera->is_editor_model_visible = true;
#endif
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
    camera->custom_value = 0;
    camera->is_view_mat_outdated = true;
    camera->is_proj_mat_outdated = true;
    camera->is_directions_outdated = true;
    camera->is_serialization_allowed = true;
    camera->custom_on_before_destroyed = NULL;
    camera->custom_ptr = NULL;

    return camera;
}

void
camera_destroy(te_camera* camera) {
    if (camera->custom_on_before_destroyed != NULL) {
        camera->custom_on_before_destroyed(camera);
    }

    free(camera->name);

    free(camera);
}

#if defined(ENGINE_EDITOR)
static void
get_editor_camera_model_geometry(
    te_model* model, te_vertex_pack** vertices, unsigned short** indices,
    unsigned int* index_count, bool* free_geometry) {
    (void)model;

    (*free_geometry) = true;
    mesh_generator_icosphere(vertices, indices, index_count);
}

static void
create_editor_model(te_camera* camera) {
    if (camera->world == NULL || camera->editor_model != NULL) {
        return;
    }

    camera->editor_model = model_create();

    model_set_is_serialization_allowed(camera->editor_model, false);
    model_set_color(camera->editor_model, (vec4){0.5f, 0.5f, 0.5f, 1.0f});
    model_set_scale(camera->editor_model, (vec3){0.25f, 0.25f, 0.25f});
    model_set_custom_geometry_provider(camera->editor_model, get_editor_camera_model_geometry);
    model_set_custom_ptr(camera->editor_model, camera);

    world_spawn_game_object(camera->world, camera->editor_model, model_get_game_object_info());

    vec3 pos;
    camera_get_world_position(camera, pos);
    model_set_position(camera->editor_model, pos);
}

static void
destroy_editor_model(te_camera* camera) {
    if (camera->editor_model == NULL || camera->world == NULL) {
        return;
    }

    world_despawn_game_object(
        camera->world, camera->editor_model, model_get_game_object_info());
    model_destroy(camera->editor_model);
    camera->editor_model = NULL;
}
#endif

static void
on_spawned(te_camera* camera, struct te_world* world) {
    if (world == camera->world) {
        return;
    }

    if (world == NULL) {
        log_error("expected world to be valid");
        abort();
    }

    camera->world = world;

#if defined(ENGINE_EDITOR)
    create_editor_model(camera);
#endif
}

static void
on_despawned(te_camera* camera) {
#if defined(ENGINE_EDITOR)
    if (camera->world != NULL && camera->editor_model != NULL) {
        if (!prv_world_is_being_destroyed(camera->world)) {
            world_despawn_game_object(
                camera->world, camera->editor_model, model_get_game_object_info());
            model_destroy(camera->editor_model);
        }
        camera->editor_model = NULL;
    }
#endif

    camera->world = NULL;
}

te_game_object_info*
camera_get_game_object_info() {
    return type_database_get_type_info(camera_get_type_id())->game_object_info;
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
type_spawn(te_world* world, te_camera* camera) {
    if (camera->world != NULL) {
        log_error("the camera is already spawned in the different world");
        abort();
    }

    world_spawn_game_object(world, camera, camera_get_game_object_info());
}

static void
type_despawn(te_world* world, te_camera* camera) {
    if (camera->world != world) {
        log_error("the model is spawned in the different world");
        abort();
    }

    if (camera->parent_model != NULL) {
        model_attach_camera(
            camera->parent_model,
            NULL); // make camera to be in the array of root world objects
    }
    world_despawn_game_object(
        camera->world, camera, camera_get_game_object_info()); // despawn root world object
}

void
camera_register_type(void) {
    te_game_object_info* game_object_info = malloc(sizeof(te_game_object_info));
    game_object_info->type_id = camera_get_type_id();
    game_object_info->type = TE_GOT_CAMERA;
    game_object_info->set_position = camera_set_position;
    game_object_info->get_position = camera_get_position;
    game_object_info->get_world = camera_get_world;
    game_object_info->get_name = camera_get_name;
    game_object_info->on_spawned = on_spawned;
    game_object_info->on_despawned = on_despawned;
    game_object_info->destroy = camera_destroy;

    te_type_info* info = type_info_create(
        camera_get_type_id(), camera_create, camera_destroy, type_spawn, type_despawn, NULL,
        game_object_info, camera_is_serialization_allowed);

    type_info_add_vec3_variable(info, "position", camera_set_position, camera_get_position);
    type_info_add_vec3_variable(info, "rotation", camera_set_rotation, camera_get_rotation);
    type_info_add_uint_variable(
        info, "vertical_fov", camera_set_vertical_fov, camera_get_vertical_fov);
    type_info_add_uint_variable(
        info, "custom_value", camera_set_custom_value, camera_get_custom_value);
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

#if defined(ENGINE_EDITOR)
    if (camera->editor_model != NULL) {
        vec3 pos;
        camera_get_world_position(camera, pos);
        model_set_position(camera->editor_model, pos);
    }
#endif
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
camera_get_world_position(te_camera* camera, vec3 out) {
    // Get camera world pos.
    vec4 camera_pos;
    camera_pos[3] = 1.0f;
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

void
camera_set_custom_ptr(te_camera* camera, void* ptr) {
    camera->custom_ptr = ptr;
}

void*
camera_get_custom_ptr(te_camera* camera) {
    return camera->custom_ptr;
}

void
camera_set_custom_value(te_camera* camera, unsigned int value) {
    camera->custom_value = value;
}

unsigned int
camera_get_custom_value(te_camera* camera) {
    return camera->custom_value;
}

void
camera_set_custom_on_before_destroyed(
    te_camera* camera, void (*custom_on_before_destroyed)(te_camera*)) {
    camera->custom_on_before_destroyed = custom_on_before_destroyed;
}

bool
camera_calc_cursor_world_dir(te_camera* camera, vec2 cursor_relative_pos, vec3 out) {
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
    vec3 forward;
    vec3 up;
    camera_get_forward(camera, forward);
    camera_get_up(camera, up);

    vec3 pos;
    camera_get_world_position(camera, pos);

    camera->frustum = frustum_shape_create(
        pos, forward, up, camera->near_clip, camera->far_clip, camera->vertical_fov,
        (float)camera->render_width / (float)camera->render_height);
}

static void
make_sure_view_proj_mat_updated(te_camera* camera) {
    if (camera->is_view_mat_outdated || camera->is_proj_mat_outdated) {
        if (camera->is_view_mat_outdated) {
            vec3 forward;
            vec3 up;
            camera_get_forward(camera, forward);
            camera_get_up(camera, up);

            vec3 pos;
            camera_get_world_position(camera, pos);

            glm_look_rh(pos, forward, up, camera->view_mat);
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
}

mat4*
camera_get_view_proj_mat(te_camera* camera) {
    make_sure_view_proj_mat_updated(camera);
    return &camera->view_proj_mat;
}

mat4*
camera_get_view_mat(te_camera* camera) {
    make_sure_view_proj_mat_updated(camera);
    return &camera->view_mat;
}

mat4*
camera_get_proj_mat(te_camera* camera) {
    make_sure_view_proj_mat_updated(camera);
    return &camera->proj_mat;
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

    // This makes sure the frustum is recalculated if needed.
    (void)camera_get_view_proj_mat(camera);

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
prv_camera_on_active(te_camera* camera) {
#if defined(ENGINE_EDITOR)
    destroy_editor_model(camera);
#else
    (void)camera;
#endif
}

void
prv_camera_on_deactivated(te_camera* camera) {
#if defined(ENGINE_EDITOR)
    create_editor_model(camera);
#else
    (void)camera;
#endif
}

void
prv_camera_on_parent_model_world_mat_changed(te_camera* camera, te_model* parent) {
    camera->is_directions_outdated = true;
    camera->is_view_mat_outdated = true;

    camera->parent_model = parent;

#if defined(ENGINE_EDITOR)
    if (camera->editor_model != NULL) {
        vec3 pos;
        camera_get_world_position(camera, pos);
        model_set_position(camera->editor_model, pos);
    }
#endif
}

#if defined(ENGINE_EDITOR)
void
prv_camera_set_editor_shape_visibility(te_camera* camera, bool is_visible) {
    camera->is_editor_model_visible = is_visible;

    if (camera->editor_model != NULL && !is_visible) {
        destroy_editor_model(camera);
        return;
    }

    if (camera->editor_model == NULL && is_visible && camera->world != NULL) {
        create_editor_model(camera);
        return;
    }
}
bool
prv_camera_is_editor_shape_visible(te_camera* camera) {
    return camera->editor_model != NULL;
}
#endif
