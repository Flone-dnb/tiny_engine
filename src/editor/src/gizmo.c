#include <gizmo.h>

#include <game/model.h>
#include <world.h>
#include <game/camera.h>
#include <math_funcs.h>

struct te_gizmo {
    te_model* model_x;
    te_model* model_y;
    te_model* model_z;

    te_model* target;

    enum te_gizmo_mode mode;

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
create_gizmo_model(te_gizmo* gizmo, unsigned int axis_idx) {
    te_model* model = model_create();

    model_set_is_serialization_allowed(model, false);
    model_set_custom_ptr(model, gizmo);
    model_set_custom_value(model, axis_idx);
    model_set_custom_geometry_provider(model, get_geometry);
    model_set_custom_vert_shader(model, "editor/shader/gizmo.vert.glsl");

    return model;
}

static void update_gizmo_rotation(te_gizmo* gizmo) {
    vec3 rot;
    te_model* parent = model_get_parent(gizmo->target);
    if (parent == NULL) {
        if (gizmo->mode == TE_GM_ROTATE || gizmo->mode == TE_GM_SCALE) {
            model_get_rotation(gizmo->target, rot);
        } else {
            glm_vec3_zero(rot);
        }
    } else {
        model_get_rotation(parent, rot);
    }

    model_set_rotation(gizmo->model_x, rot);
    model_set_rotation(gizmo->model_y, rot);
    model_set_rotation(gizmo->model_z, rot);
}

static void
spawn_gizmo_models(te_gizmo* gizmo, te_world* world) {
    // Only 1 model should call this.
    model_set_custom_on_before_destroyed(gizmo->model_z, on_before_model_destroyed);

    world_spawn_game_object(world, model_get_game_object_info(gizmo->model_x));
    world_spawn_game_object(world, model_get_game_object_info(gizmo->model_y));
    world_spawn_game_object(world, model_get_game_object_info(gizmo->model_z));

    vec3 target_pos;
    model_get_world_position(gizmo->target, target_pos);

    model_set_position(gizmo->model_x, target_pos);
    model_set_position(gizmo->model_y, target_pos);
    model_set_position(gizmo->model_z, target_pos);

    model_set_color(gizmo->model_x, (vec4){1.0f, 0.0f, 0.0f, 1.0f});
    model_set_color(gizmo->model_y, (vec4){0.0f, 1.0f, 0.0f, 1.0f});
    model_set_color(gizmo->model_z, (vec4){0.0f, 0.0f, 1.0f, 1.0f});

    update_gizmo_rotation(gizmo);
}

te_gizmo*
gizmo_create_in_world(te_world* world, te_model* target) {
    te_gizmo* gizmo = malloc(sizeof(te_gizmo));
    gizmo->model_x = create_gizmo_model(gizmo, 0);
    gizmo->model_y = create_gizmo_model(gizmo, 1);
    gizmo->model_z = create_gizmo_model(gizmo, 2);
    gizmo->mode = TE_GM_MOVE;
    gizmo->grab_x = false;
    gizmo->grab_y = false;
    gizmo->grab_z = false;
    gizmo->target = target;

    spawn_gizmo_models(gizmo, world);

    return gizmo;
}

void
gizmo_destroy_in_world_now(te_gizmo* gizmo, te_world* world) {
    world_despawn_game_object(world, model_get_game_object_info(gizmo->model_x));
    world_despawn_game_object(world, model_get_game_object_info(gizmo->model_y));
    world_despawn_game_object(world, model_get_game_object_info(gizmo->model_z));

    model_destroy(gizmo->model_x);
    model_destroy(gizmo->model_y);
    model_destroy(gizmo->model_z); // <- triggers gizmo destroy
}

void*
gizmo_get_target(te_gizmo* gizmo) {
    return gizmo->target;
}

void
gizmo_set_mode(te_gizmo* gizmo, enum te_gizmo_mode mode) {
    if (gizmo->mode == mode) {
        return;
    }

    gizmo->mode = mode;

    model_set_custom_on_before_destroyed(gizmo->model_z, NULL);

    te_world* world = model_get_world(gizmo->model_x);

    world_despawn_game_object(world, model_get_game_object_info(gizmo->model_x));
    world_despawn_game_object(world, model_get_game_object_info(gizmo->model_y));
    world_despawn_game_object(world, model_get_game_object_info(gizmo->model_z));

    model_destroy(gizmo->model_x);
    model_destroy(gizmo->model_y);
    model_destroy(gizmo->model_z);

    gizmo->model_x = create_gizmo_model(gizmo, 0);
    gizmo->model_y = create_gizmo_model(gizmo, 1);
    gizmo->model_z = create_gizmo_model(gizmo, 2);

    spawn_gizmo_models(gizmo, world);
}

enum te_gizmo_mode gizmo_get_mode(te_gizmo* gizmo) {
    return gizmo->mode;
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

    mat4* world_mat = prv_model_get_world_mat_tmp(gizmo->target);

    vec3 gizmo_offset;
    glm_vec3_zero(gizmo_offset);
    if (gizmo->grab_x) {
        gizmo_offset[0] = glm_vec3_dot(dir, (*world_mat)[0]);
    } else if (gizmo->grab_y) {
        gizmo_offset[1] = glm_vec3_dot(dir, (*world_mat)[1]);
    } else if (gizmo->grab_z) {
        gizmo_offset[2] = glm_vec3_dot(dir, (*world_mat)[2]);
    }


    if (gizmo->mode == TE_GM_MOVE) {
        glm_vec3_scale(
            gizmo_offset, 0.01f * fabsf(x_offset + y_offset),
            gizmo_offset);

        vec3 pos;
        model_get_position(gizmo->target, pos);
        glm_vec3_add(pos, gizmo_offset, pos);

        model_set_position(gizmo->target, pos);

        vec3 target_pos;
        model_get_world_position(gizmo->target, target_pos);
        model_set_position(gizmo->model_x, target_pos);
        model_set_position(gizmo->model_y, target_pos);
        model_set_position(gizmo->model_z, target_pos);
    } else if (gizmo->mode == TE_GM_ROTATE) {
        glm_vec3_scale(gizmo_offset, 0.5f * fabsf(x_offset + y_offset), gizmo_offset);

        vec3 rot;
        model_get_rotation(gizmo->target, rot);
        glm_vec3_add(rot, gizmo_offset, rot);

        model_set_rotation(gizmo->target, rot);

        update_gizmo_rotation(gizmo);
    } else if (gizmo->mode == TE_GM_SCALE) {
        glm_vec3_scale(gizmo_offset, 0.005f * fabsf(x_offset + y_offset), gizmo_offset);

        vec3 scale;
        model_get_scale(gizmo->target, scale);
        glm_vec3_add(scale, gizmo_offset, scale);

        model_set_scale(gizmo->target, scale);
    }
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

static void generate_base(
    float half_width, float half, te_model_vertex* start_vertex, unsigned short* start_index, unsigned short index_offset);

static void
get_geometry(
    te_model* model, te_model_vertex** vertices, unsigned short** indices,
    unsigned int* vertex_count, unsigned int* index_count, bool* free_geometry) {
    te_gizmo* gizmo = model_get_custom_ptr(model);

    // Create geometry of single axis.
    (*free_geometry) = true;

    const float half_width = 1.0f;
    const float half = 0.125f;

    (*vertex_count) = gizmo->mode == TE_GM_MOVE ? 24 : 24 * 2;
    (*vertices) = malloc(sizeof(te_model_vertex) * (*vertex_count));

    (*index_count) = gizmo->mode == TE_GM_MOVE ? 36 : 36 * 2;
    (*indices) = malloc(sizeof(unsigned short) * (*index_count));

    generate_base(half_width, half, (*vertices), (*indices), 0);

    if (gizmo->mode == TE_GM_ROTATE) {
        // Add new shape near the origin.
        generate_base(half * 2.0f, half * 2.0f, (*vertices) + 24, (*indices) + 36, 24);

        for (unsigned int k = 24; k < 24 * 2; k++) {
            glm_vec3_add((*vertices)[k].pos, (vec3){half * 2.0f, 0.0f, 0.0f}, (*vertices)[k].pos);
        }
    } else if (gizmo->mode == TE_GM_SCALE) {
        // Add new shape.
        generate_base(half * 2.0f, half * 2.0f, (*vertices) + 24, (*indices) + 36, 24);

        for (unsigned int k = 24; k < 24 * 2; k++) {
            glm_vec3_add(
                (*vertices)[k].pos, (vec3){half_width * 2.0f + half, 0.0f, 0.0f},
                (*vertices)[k].pos);
        }
    }

    // Rotate according to the axis.
    const size_t axis_idx = model_get_custom_value(model);
    if (axis_idx > 0) {
        mat4 rot_mat;
        if (axis_idx == 1) {
            math_make_rotation_mat((vec3){0.0f, 0.0f, 90.0f}, rot_mat);
        } else {
            math_make_rotation_mat((vec3){0.0f, -90.0f, 0.0f}, rot_mat);
        }

        for (unsigned int k = 0; k < (*vertex_count); k++) {
            vec4 pos;
            glm_vec3_copy((*vertices)[k].pos, pos);
            pos[3] = 1.0f;

            glm_mat4_mulv(rot_mat, pos, pos);

            glm_vec3_copy(pos, (*vertices)[k].pos);
        }
    }
}

static void
generate_base(
    float half_width, float half, te_model_vertex* start_vertex, unsigned short* start_index, unsigned short index_offset) {
    // Init UVs.
    for (unsigned int i = 0; i < 24; i += 4) {
        glm_vec2_make((vec2){1.0f, 1.0f}, start_vertex[i].uv);
        glm_vec2_make((vec2){0.0f, 1.0f}, start_vertex[i + 1].uv);
        glm_vec2_make((vec2){1.0f, 0.0f}, start_vertex[i + 2].uv);
        glm_vec2_make((vec2){0.0f, 0.0f}, start_vertex[i + 3].uv);
    }

    // Init normals.
    unsigned int normal_i = 0;
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){1.0f, 0.0f, 0.0f}, start_vertex[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){-1.0f, 0.0f, 0.0f}, start_vertex[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){0.0f, 1.0f, 0.0f}, start_vertex[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){0.0f, -1.0f, 0.0f}, start_vertex[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){0.0f, 0.0f, 1.0f}, start_vertex[normal_i].normal);
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make((vec3){.0f, 0.0f, -1.0f}, start_vertex[normal_i].normal);
    }

    // Init positions.

    // +X face.
    unsigned int i = 0;
    glm_vec3_make((vec3){half_width, -half, -half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, half, -half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, -half, half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, half, half}, start_vertex[i].pos);
    i += 1;

    // -X face.
    glm_vec3_make((vec3){-half_width, half, -half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, -half, -half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, half, half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, -half, half}, start_vertex[i].pos);
    i += 1;

    // +Y face.
    glm_vec3_make((vec3){half_width, half, -half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, half, -half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, half, half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, half, half}, start_vertex[i].pos);
    i += 1;

    // -Y face.
    glm_vec3_make((vec3){-half_width, -half, -half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, -half, -half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, -half, half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, -half, half}, start_vertex[i].pos);
    i += 1;

    // +Z face.
    glm_vec3_make((vec3){-half_width, -half, half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, -half, half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, half, half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, half, half}, start_vertex[i].pos);
    i += 1;

    // -Z face.
    glm_vec3_make((vec3){-half_width, half, -half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, half, -half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){-half_width, -half, -half}, start_vertex[i].pos);
    i += 1;
    glm_vec3_make((vec3){half_width, -half, -half}, start_vertex[i].pos);
    i += 1;

    // Make origin around 0.
    for (unsigned int k = 0; k < 24; k++) {
        start_vertex[k].pos[0] += half_width + half_width / 4.0f;
    }

    start_index[0] = index_offset + 0; // +X face.
    start_index[1] = index_offset + 1;
    start_index[2] = index_offset + 2;
    start_index[3] = index_offset + 3;
    start_index[4] = index_offset + 2;
    start_index[5] = index_offset + 1;
    start_index[6] = index_offset + 4; // -X face.
    start_index[7] = index_offset + 5;
    start_index[8] = index_offset + 6;
    start_index[9] = index_offset + 7;
    start_index[10] = index_offset + 6;
    start_index[11] = index_offset + 5;
    start_index[12] = index_offset + 8; // +Y face.
    start_index[13] = index_offset + 9;
    start_index[14] = index_offset + 10;
    start_index[15] = index_offset + 11;
    start_index[16] = index_offset + 10;
    start_index[17] = index_offset + 9;
    start_index[18] = index_offset + 12; // -Y face.
    start_index[19] = index_offset + 13;
    start_index[20] = index_offset + 14;
    start_index[21] = index_offset + 15;
    start_index[22] = index_offset + 14;
    start_index[23] = index_offset + 13;
    start_index[24] = index_offset + 16; // +Z face.
    start_index[25] = index_offset + 17;
    start_index[26] = index_offset + 18;
    start_index[27] = index_offset + 19;
    start_index[28] = index_offset + 18;
    start_index[29] = index_offset + 17;
    start_index[30] = index_offset + 20; // -Z face.
    start_index[31] = index_offset + 21;
    start_index[32] = index_offset + 22;
    start_index[33] = index_offset + 23;
    start_index[34] = index_offset + 22;
    start_index[35] = index_offset + 21;
}