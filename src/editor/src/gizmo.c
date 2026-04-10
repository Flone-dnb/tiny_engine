#include <gizmo.h>

#include <game/model.h>
#include <world.h>
#include <game/camera.h>

struct te_gizmo {
    te_model* model_x;
    te_model* model_y;
    te_model* model_z;

    te_model* target;

    bool grab_x;
    bool grab_y;
    bool grab_z;
};

static void
on_before_model_destroyed(te_model* model) {
    te_gizmo* gizmo = model_get_custom_ptr(model);

    free(gizmo);
}

static void get_geometry(
    te_model* model, te_model_vertex** vertices, unsigned short** indices,
    unsigned int* vertex_count, unsigned int* index_count, bool* free_geometry);

static te_model*
create_gizmo_model(te_gizmo* gizmo) {
    te_model* model = model_create();

    model_set_is_serialization_allowed(model, false);
    model_set_custom_ptr(model, gizmo);
    model_set_custom_geometry_provider(model, get_geometry);
    model_set_custom_vert_shader(model, "editor/shader/gizmo.vert.glsl");

    return model;
}

te_gizmo*
gizmo_create_in_world(struct te_world* world, te_model* target) {
    te_gizmo* gizmo = malloc(sizeof(te_gizmo));
    gizmo->model_x = create_gizmo_model(gizmo);
    gizmo->model_y = create_gizmo_model(gizmo);
    gizmo->model_z = create_gizmo_model(gizmo);
    gizmo->grab_x = false;
    gizmo->grab_y = false;
    gizmo->grab_z = false;
    gizmo->target = target;

    // Only 1 model should call this.
    model_set_custom_on_before_destroyed(gizmo->model_z, on_before_model_destroyed);

    world_spawn_model(world, gizmo->model_x);
    world_spawn_model(world, gizmo->model_y);
    world_spawn_model(world, gizmo->model_z);

    vec3 target_pos;
    model_get_position(target, target_pos);

    model_set_position(gizmo->model_x, target_pos);
    model_set_position(gizmo->model_y, target_pos);
    model_set_position(gizmo->model_z, target_pos);

    model_set_rotation(gizmo->model_y, (vec3){0.0f, 0.0f, 90.0f});
    model_set_rotation(gizmo->model_z, (vec3){0.0f, -90.0f, 0.0f});

    model_set_color(gizmo->model_x, (vec4){1.0f, 0.0f, 0.0f, 1.0f});
    model_set_color(gizmo->model_y, (vec4){0.0f, 1.0f, 0.0f, 1.0f});
    model_set_color(gizmo->model_z, (vec4){0.0f, 0.0f, 1.0f, 1.0f});

    return gizmo;
}

void
gizmo_destroy_in_world_now(te_gizmo* gizmo, te_world* world) {
    world_despawn_model(world, gizmo->model_x);
    world_despawn_model(world, gizmo->model_y);
    world_despawn_model(world, gizmo->model_z);

    model_destroy(gizmo->model_x);
    model_destroy(gizmo->model_y);
    model_destroy(gizmo->model_z); // <- triggers gizmo destroy
}

void*
gizmo_get_target(te_gizmo* gizmo) {
    return gizmo->target;
}

void
gizmo_start_grab_x(te_gizmo* gizmo) {
    gizmo->grab_x = true;
}

void
gizmo_start_grab_y(te_gizmo* gizmo) {
    gizmo->grab_y = true;
}

void
gizmo_start_grab_z(te_gizmo* gizmo) {
    gizmo->grab_z = true;
}

void
gizmo_end_grab(te_gizmo* gizmo) {
    gizmo->grab_x = false;
    gizmo->grab_y = false;
    gizmo->grab_z = false;
}

bool
gizmo_is_grabbed(te_gizmo* gizmo) {
    return gizmo->grab_x || gizmo->grab_y || gizmo->grab_z;
}

void
gizmo_move(te_gizmo* gizmo, te_camera* camera, float x_offset, float y_offset) {
    y_offset *= -1.0f;

    vec3 right;
    camera_get_right(camera, right);

    vec3 up;
    camera_get_up(camera, up);

    glm_vec3_scale(right, x_offset, right);
    glm_vec3_scale(up, y_offset, up);

    vec3 dir;
    glm_vec3_add(right, up, dir);
    glm_vec3_normalize(dir);

    vec3 gizmo_offset;
    glm_vec3_zero(gizmo_offset);
    if (gizmo->grab_x) {
        gizmo_offset[0] = glm_vec3_dot(dir, (vec3){1.0f, 0.0f, 0.0f});
    } else if (gizmo->grab_y) {
        gizmo_offset[1] = glm_vec3_dot(dir, (vec3){0.0f, 1.0f, 0.0f});
    } else if (gizmo->grab_z) {
        gizmo_offset[2] = glm_vec3_dot(dir, (vec3){0.0f, 0.0f, 1.0f});
    }
    glm_vec3_scale(
        gizmo_offset, 0.01f * fabsf(x_offset + y_offset),
        gizmo_offset); // apply movement speed

    vec3 target_pos;
    model_get_position(gizmo->target, target_pos);

    glm_vec3_add(target_pos, gizmo_offset, target_pos);

    model_set_position(gizmo->target, target_pos);

    model_set_position(gizmo->model_x, target_pos);
    model_set_position(gizmo->model_y, target_pos);
    model_set_position(gizmo->model_z, target_pos);

    model_set_rotation(gizmo->model_y, (vec3){0.0f, 0.0f, 90.0f});
    model_set_rotation(gizmo->model_z, (vec3){0.0f, -90.0f, 0.0f});
}

te_model*
gizmo_get_model_x(te_gizmo* gizmo) {
    return gizmo->model_x;
}

te_model*
gizmo_get_model_y(te_gizmo* gizmo) {
    return gizmo->model_y;
}

te_model*
gizmo_get_model_z(te_gizmo* gizmo) {
    return gizmo->model_z;
}

static void
get_geometry(
    te_model* model, te_model_vertex** vertices, unsigned short** indices,
    unsigned int* vertex_count, unsigned int* index_count, bool* free_geometry) {
    (void)model;

    // Geometry of single axis gizmo.
    // Start with a wide cube and then add an arrow.
    (*free_geometry) = true;

    const float half_width = 1.0f;
    const float half = 0.125f;

    (*vertex_count) = 24;
    (*vertices) = malloc(sizeof(te_model_vertex) * (*vertex_count));

    // Init UVs.
    for (unsigned int i = 0; i < (*vertex_count); i += 4) {
        glm_vec2_make((vec2){1.0f, 1.0f}, (*vertices)[i].uv);
        glm_vec2_make((vec2){0.0f, 1.0f}, (*vertices)[i + 1].uv);
        glm_vec2_make((vec2){1.0f, 0.0f}, (*vertices)[i + 2].uv);
        glm_vec2_make((vec2){0.0f, 0.0f}, (*vertices)[i + 3].uv);
    }

    // Init normals.
    unsigned int normal_i = 0;
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){1.0f, 0.0f, 0.0f}, (*vertices)[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){-1.0f, 0.0f, 0.0f}, (*vertices)[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){0.0f, 1.0f, 0.0f}, (*vertices)[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){0.0f, -1.0f, 0.0f}, (*vertices)[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){0.0f, 0.0f, 1.0f}, (*vertices)[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){.0f, 0.0f, -1.0f}, (*vertices)[normal_i].normal);
    }

    // Init positions.

    // +X face.
    unsigned int i = 0;
    glm_vec3_make((vec3){half_width, -half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, -half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, half, half}, (*vertices)[i].pos);
    i += 1;

    // -X face.
    glm_vec3_make((vec3){-half_width, half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, -half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, -half, half}, (*vertices)[i].pos);
    i += 1;

    // +Y face.
    glm_vec3_make((vec3){half_width, half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, half, half}, (*vertices)[i].pos);
    i += 1;

    // -Y face.
    glm_vec3_make((vec3){-half_width, -half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, -half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, -half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, -half, half}, (*vertices)[i].pos);
    i += 1;

    // +Z face.
    glm_vec3_make((vec3){-half_width, -half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, -half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, half, half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, half, half}, (*vertices)[i].pos);
    i += 1;

    // -Z face.
    glm_vec3_make((vec3){-half_width, half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, -half, -half}, (*vertices)[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, -half, -half}, (*vertices)[i].pos);
    i += 1;

    // Make origin around 0.
    for (unsigned int k = 0; k < (*vertex_count); k++) {
        (*vertices)[k].pos[0] += half_width + half_width / 4.0f;
    }

    (*index_count) = 36;
    (*indices) = malloc(sizeof(unsigned short) * (*index_count));
    (*indices)[0] = 0; // +X face.
    (*indices)[1] = 1;
    (*indices)[2] = 2;
    (*indices)[3] = 3;
    (*indices)[4] = 2;
    (*indices)[5] = 1;
    (*indices)[6] = 4; // -X face.
    (*indices)[7] = 5;
    (*indices)[8] = 6;
    (*indices)[9] = 7;
    (*indices)[10] = 6;
    (*indices)[11] = 5;
    (*indices)[12] = 8; // +Y face.
    (*indices)[13] = 9;
    (*indices)[14] = 10;
    (*indices)[15] = 11;
    (*indices)[16] = 10;
    (*indices)[17] = 9;
    (*indices)[18] = 12; // -Y face.
    (*indices)[19] = 13;
    (*indices)[20] = 14;
    (*indices)[21] = 15;
    (*indices)[22] = 14;
    (*indices)[23] = 13;
    (*indices)[24] = 16; // +Z face.
    (*indices)[25] = 17;
    (*indices)[26] = 18;
    (*indices)[27] = 19;
    (*indices)[28] = 18;
    (*indices)[29] = 17;
    (*indices)[30] = 20; // -Z face.
    (*indices)[31] = 21;
    (*indices)[32] = 22;
    (*indices)[33] = 23;
    (*indices)[34] = 22;
    (*indices)[35] = 21;
}
