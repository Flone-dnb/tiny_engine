#include <gizmo.h>

#include <stdlib.h>
#include <game/model.h>
#include <world.h>
#include <game/camera.h>
#include <math/math_funcs.h>

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
    te_model* model, te_vertex_pack** vertices, unsigned short** indices,
    unsigned int* index_count, bool* free_geometry);

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

static void
update_gizmo_rotation(te_gizmo* gizmo) {
    te_vec3 rot;
    te_model* parent = model_get_parent(gizmo->target);
    if (parent == NULL) {
        if (gizmo->mode == TE_GM_ROTATE || gizmo->mode == TE_GM_SCALE) {
            model_get_rotation(gizmo->target, rot);
        } else {
            vec3_zero(rot);
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
    te_vec4 color;
    te_vec3 target_pos;

    /* only 1 model should call this */
    model_set_custom_on_before_destroyed(gizmo->model_z, on_before_model_destroyed);

    world_spawn_game_object(world, gizmo->model_x, model_get_game_object_info());
    world_spawn_game_object(world, gizmo->model_y, model_get_game_object_info());
    world_spawn_game_object(world, gizmo->model_z, model_get_game_object_info());

    model_get_world_position(gizmo->target, target_pos);

    model_set_position(gizmo->model_x, target_pos);
    model_set_position(gizmo->model_y, target_pos);
    model_set_position(gizmo->model_z, target_pos);

    vec4_set(1.0f, 0.0f, 0.0f, 1.0f, color);
    model_set_color(gizmo->model_x, color);

    vec4_set(0.0f, 1.0f, 0.0f, 1.0f, color);
    model_set_color(gizmo->model_y, color);

    vec4_set(0.0f, 0.0f, 1.0f, 1.0f, color);
    model_set_color(gizmo->model_z, color);

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
    world_despawn_game_object(world, gizmo->model_x, model_get_game_object_info());
    world_despawn_game_object(world, gizmo->model_y, model_get_game_object_info());
    world_despawn_game_object(world, gizmo->model_z, model_get_game_object_info());

    model_destroy(gizmo->model_x);
    model_destroy(gizmo->model_y);
    model_destroy(gizmo->model_z); /* triggers gizmo destroy */
}

void*
gizmo_get_target(te_gizmo* gizmo) {
    return gizmo->target;
}

void
gizmo_set_mode(te_gizmo* gizmo, enum te_gizmo_mode mode) {
    te_world* world;

    if (gizmo->mode == mode) {
        return;
    }

    gizmo->mode = mode;

    model_set_custom_on_before_destroyed(gizmo->model_z, NULL);

    world = model_get_world(gizmo->model_x);

    world_despawn_game_object(world, gizmo->model_x, model_get_game_object_info());
    world_despawn_game_object(world, gizmo->model_y, model_get_game_object_info());
    world_despawn_game_object(world, gizmo->model_z, model_get_game_object_info());

    model_destroy(gizmo->model_x);
    model_destroy(gizmo->model_y);
    model_destroy(gizmo->model_z);

    gizmo->model_x = create_gizmo_model(gizmo, 0);
    gizmo->model_y = create_gizmo_model(gizmo, 1);
    gizmo->model_z = create_gizmo_model(gizmo, 2);

    spawn_gizmo_models(gizmo, world);
}

enum te_gizmo_mode
gizmo_get_mode(te_gizmo* gizmo) {
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
    te_vec3 right;
    te_vec3 up;
    te_vec3 dir;
    te_vec3 gizmo_offset;
    te_mat4* world_mat;

    y_offset *= -1.0f;

    camera_get_right(camera, right);

    camera_get_up(camera, up);

    vec3_muls(right, x_offset, right);
    vec3_muls(up, y_offset, up);

    vec3_add(right, up, dir);
    vec3_normalize(dir);

    world_mat = prv_model_get_world_mat_tmp(gizmo->target);

    vec3_zero(gizmo_offset);
    if (gizmo->grab_x) {
        gizmo_offset[0] = vec3_dot(dir, (*world_mat)[0]);
    } else if (gizmo->grab_y) {
        gizmo_offset[1] = vec3_dot(dir, (*world_mat)[1]);
    } else if (gizmo->grab_z) {
        gizmo_offset[2] = vec3_dot(dir, (*world_mat)[2]);
    }

    if (gizmo->mode == TE_GM_MOVE) {
        te_vec3 pos;
        te_vec3 target_pos;

        vec3_muls(gizmo_offset, 0.01f * math_abs(x_offset + y_offset), gizmo_offset);

        model_get_position(gizmo->target, pos);
        vec3_add(pos, gizmo_offset, pos);

        model_set_position(gizmo->target, pos);

        model_get_world_position(gizmo->target, target_pos);
        model_set_position(gizmo->model_x, target_pos);
        model_set_position(gizmo->model_y, target_pos);
        model_set_position(gizmo->model_z, target_pos);
    } else if (gizmo->mode == TE_GM_ROTATE) {
        te_vec3 rot;

        vec3_muls(gizmo_offset, 0.5f * math_abs(x_offset + y_offset), gizmo_offset);

        model_get_rotation(gizmo->target, rot);
        vec3_add(rot, gizmo_offset, rot);

        model_set_rotation(gizmo->target, rot);

        update_gizmo_rotation(gizmo);
    } else if (gizmo->mode == TE_GM_SCALE) {
        te_vec3 scale;

        vec3_muls(gizmo_offset, 0.005f * math_abs(x_offset + y_offset), gizmo_offset);

        model_get_scale(gizmo->target, scale);
        vec3_add(scale, gizmo_offset, scale);

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
    float half_width, float half, te_vertex_pack* vertices, unsigned short* indices,
    unsigned int start_vertex, unsigned int start_index, unsigned short index_offset);

static void
get_geometry(
    te_model* model, te_vertex_pack** vertices, unsigned short** indices,
    unsigned int* index_count, bool* free_geometry) {
    size_t axis_idx;
    unsigned int vert_count;
    unsigned int k;

    const float half_width = 1.0f;
    const float half = 0.125f;

    te_gizmo* gizmo = model_get_custom_ptr(model);

    /* create geometry of single axis */
    (*free_geometry) = true;

    vert_count = gizmo->mode == TE_GM_MOVE ? 24 : 24 * 2;
    (*vertices) = vertex_pack_create(vert_count, false);

    (*index_count) = gizmo->mode == TE_GM_MOVE ? 36 : 36 * 2;
    (*indices) = malloc(sizeof(unsigned short) * (*index_count));

    generate_base(half_width, half, (*vertices), (*indices), 0, 0, 0);

    if (gizmo->mode == TE_GM_ROTATE) {
        /* add new shape near the origin */
        generate_base(half * 2.0f, half * 2.0f, (*vertices), (*indices), 24, 36, 24);

        for (k = 24; k < 24 * 2; k++) {
            unsigned char* data = (*vertices)->data
                                  + ((*vertices)->vertex_sizeof * k
                                     + (*vertices)->attribute_offsets[TE_VA_POSITION]);
            *(float*)data += half * 2.0f; /* changing X of a vec3 */
        }
    } else if (gizmo->mode == TE_GM_SCALE) {
        /* add new shape */
        generate_base(half * 2.0f, half * 2.0f, (*vertices), (*indices), 24, 36, 24);

        for (k = 24; k < 24 * 2; k++) {
            unsigned char* data = (*vertices)->data
                                  + ((*vertices)->vertex_sizeof * k
                                     + (*vertices)->attribute_offsets[TE_VA_POSITION]);
            *(float*)data += half_width * 2.0f + half; /* changing X of a vec3 */
        }
    }

    /* rotate according to the axis */
    axis_idx = model_get_custom_value(model);
    if (axis_idx > 0) {
        te_mat4 rot_mat;
        te_vec3 rotation_deg;
        if (axis_idx == 1) {
            vec3_set(0.0f, 0.0f, 90.0f, rotation_deg);
            math_make_rotation_mat(rotation_deg, rot_mat);
        } else {
            vec3_set(0.0f, -90.0f, 0.0f, rotation_deg);
            math_make_rotation_mat(rotation_deg, rot_mat);
        }

        for (k = 0; k < vert_count; k++) {
            unsigned char* data = (*vertices)->data
                                  + ((*vertices)->vertex_sizeof * k
                                     + (*vertices)->attribute_offsets[TE_VA_POSITION]);

            te_vec4 pos;
            vec3_copy((float*)data, pos);
            pos[3] = 1.0f;

            mat4_mulv(rot_mat, pos, pos);

            vec3_copy(pos, (float*)data);
        }
    }
}

static void
generate_base(
    float half_width, float half, te_vertex_pack* vertices, unsigned short* indices,
    unsigned int start_vertex, unsigned int start_index, unsigned short index_offset) {
    unsigned int i;
    unsigned int normal_i;
    unsigned char offset;

    const unsigned int vert_size = vertices->vertex_sizeof;

    /* init UVs */
    offset = vertices->attribute_offsets[TE_VA_UV];
    for (i = 0; i < 24; i += 4) {
        vec2_set(
            1.0f, 1.0f, (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
        vec2_set(
            0.0f, 1.0f,
            (float*)(vertices->data + vert_size * (start_vertex + i + 1) + offset));
        vec2_set(
            1.0f, 0.0f,
            (float*)(vertices->data + vert_size * (start_vertex + i + 2) + offset));
        vec2_set(
            0.0f, 0.0f,
            (float*)(vertices->data + vert_size * (start_vertex + i + 3) + offset));
    }

    /* init normals */
    offset = vertices->attribute_offsets[TE_VA_NORMAL];
    normal_i = 0;
    for (i = normal_i; normal_i < i + 4; normal_i++) {
        vec3_set(
            1.0f, 0.0f, 0.0f,
            (float*)(vertices->data + vert_size * (start_vertex + normal_i) + offset));
    }
    for (i = normal_i; normal_i < i + 4; normal_i++) {
        vec3_set(
            -1.0f, 0.0f, 0.0f,
            (float*)(vertices->data + vert_size * (start_vertex + normal_i) + offset));
    }
    for (i = normal_i; normal_i < i + 4; normal_i++) {
        vec3_set(
            0.0f, 1.0f, 0.0f,
            (float*)(vertices->data + vert_size * (start_vertex + normal_i) + offset));
    }
    for (i = normal_i; normal_i < i + 4; normal_i++) {
        vec3_set(
            0.0f, -1.0f, 0.0f,
            (float*)(vertices->data + vert_size * (start_vertex + normal_i) + offset));
    }
    for (i = normal_i; normal_i < i + 4; normal_i++) {
        vec3_set(
            0.0f, 0.0f, 1.0f,
            (float*)(vertices->data + vert_size * (start_vertex + normal_i) + offset));
    }
    for (i = normal_i; normal_i < i + 4; normal_i++) {
        vec3_set(
            0.0f, 0.0f, -1.0f,
            (float*)(vertices->data + vert_size * (start_vertex + normal_i) + offset));
    }

    /* init positions */
    offset = vertices->attribute_offsets[TE_VA_POSITION];
    /* +X face */
    i = 0;
    vec3_set(
        half_width, -half, -half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        half_width, half, -half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        half_width, -half, half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        half_width, half, half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;

    /* -X face */
    vec3_set(
        -half_width, half, -half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        -half_width, -half, -half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        -half_width, half, half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        -half_width, -half, half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;

    /* +Y face */
    vec3_set(
        half_width, half, -half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        -half_width, half, -half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        half_width, half, half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        -half_width, half, half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;

    /* -Y face */
    vec3_set(
        -half_width, -half, -half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        half_width, -half, -half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        -half_width, -half, half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        half_width, -half, half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;

    /* +Z face */
    vec3_set(
        -half_width, -half, half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        half_width, -half, half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        -half_width, half, half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        half_width, half, half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;

    /* -Z face */
    vec3_set(
        -half_width, half, -half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        half_width, half, -half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        -half_width, -half, -half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;
    vec3_set(
        half_width, -half, -half,
        (float*)(vertices->data + vert_size * (start_vertex + i) + offset));
    i += 1;

    /* make origin around 0 */
    for (i = 0; i < 24; i++) {
        float* data = (float*)(vertices->data + vert_size * (start_vertex + i) + offset);
        (*data) += half_width + half_width / 4.0f; /* modify X (pos[0]) */
    }

    indices[start_index + 0] = index_offset + 0; /* +X face */
    indices[start_index + 1] = index_offset + 1;
    indices[start_index + 2] = index_offset + 2;
    indices[start_index + 3] = index_offset + 3;
    indices[start_index + 4] = index_offset + 2;
    indices[start_index + 5] = index_offset + 1;
    indices[start_index + 6] = index_offset + 4; /* -X face */
    indices[start_index + 7] = index_offset + 5;
    indices[start_index + 8] = index_offset + 6;
    indices[start_index + 9] = index_offset + 7;
    indices[start_index + 10] = index_offset + 6;
    indices[start_index + 11] = index_offset + 5;
    indices[start_index + 12] = index_offset + 8; /* +Y face */
    indices[start_index + 13] = index_offset + 9;
    indices[start_index + 14] = index_offset + 10;
    indices[start_index + 15] = index_offset + 11;
    indices[start_index + 16] = index_offset + 10;
    indices[start_index + 17] = index_offset + 9;
    indices[start_index + 18] = index_offset + 12; /* -Y face */
    indices[start_index + 19] = index_offset + 13;
    indices[start_index + 20] = index_offset + 14;
    indices[start_index + 21] = index_offset + 15;
    indices[start_index + 22] = index_offset + 14;
    indices[start_index + 23] = index_offset + 13;
    indices[start_index + 24] = index_offset + 16; /* +Z face */
    indices[start_index + 25] = index_offset + 17;
    indices[start_index + 26] = index_offset + 18;
    indices[start_index + 27] = index_offset + 19;
    indices[start_index + 28] = index_offset + 18;
    indices[start_index + 29] = index_offset + 17;
    indices[start_index + 30] = index_offset + 20; /* -Z face */
    indices[start_index + 31] = index_offset + 21;
    indices[start_index + 32] = index_offset + 22;
    indices[start_index + 33] = index_offset + 23;
    indices[start_index + 34] = index_offset + 22;
    indices[start_index + 35] = index_offset + 21;
}
