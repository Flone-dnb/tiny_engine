#include <obj_picking.h>

#include <stdlib.h>
#include <world.h>
#include <game/game_object_info.h>
#include <game/camera.h>
#include <game/model.h>
#include <game/game_object_info.h>
#include <render/model_renderer.h>
#include <shape/frustum_shape.h>
#include <gizmo.h>

typedef struct {
    te_model* model;
    te_aabb_shape aabb_world;
    float bb_size;
    float distance;
} closest_model_info;

/* updates closest model info if hit
 * returns `true` if hit gizmo and need to quit (not check other models) */
static bool
test_model_hit(
    closest_model_info* closest_info, te_frustum_shape* frustum, te_gizmo* gizmo,
    te_vec3 camera_world_pos, te_vec3 camera_world_ray, te_model* model) {
    te_model_renderer* renderer;
    te_model_render_data* data;
    float distance;
    float bb_size;

    const unsigned int handle = prv_model_get_render_data_handle(model);
    if (handle == 0xFFFFFFFF) {
        return false;
    }

    renderer = prv_model_get_model_renderer(model);
    if (renderer == NULL) {
        return false;
    }

    data = model_renderer_get_render_data_tmp(renderer, handle);
    if (!frustum_shape_is_aabb_inside(frustum, &data->aabb_world)) {
        return false;
    }

    if (!aabb_shape_intersect_ray(
            &data->aabb_world, camera_world_pos, camera_world_ray, &distance)) {
        return false;
    }

    if (gizmo != NULL) {
        if (model == gizmo_get_model_x(gizmo) || model == gizmo_get_model_y(gizmo)
            || model == gizmo_get_model_z(gizmo)) {
            /* always prioritize gizmo */
            closest_info->model = model;
            return true;
        }
    }

    bb_size = data->aabb_world.extents[0] * 2.0f * data->aabb_world.extents[1] * 2.0f
              * data->aabb_world.extents[2] * 2.0f;

    /* TODO: for now just do a bunch of simple tests (no ray-triangle intersection
     * because we don't store the geometry on the CPU) */
    if (closest_info->model != NULL) {
        if (!aabb_shape_intersect(&data->aabb_world, &closest_info->aabb_world)) {
            if (distance >= closest_info->distance) {
                return false;
            }
        } else {
            if (bb_size >= closest_info->bb_size) {
                return false;
            }
        }
    }

    closest_info->model = model;
    closest_info->aabb_world = data->aabb_world;
    closest_info->bb_size = bb_size;
    closest_info->distance = distance;

    return false;
}

void
obj_picking_find_obj_under_cursor(
    te_vec2 cursor_pos_rel, te_camera* camera, te_world* world, te_gizmo* gizmo,
    void** out_game_obj, struct te_game_object_info** out_game_obj_info) {
    te_frustum_shape* frustum;
    te_game_object_data* root_game_objects;
    te_model* model;
    closest_model_info info;
    te_vec3 camera_world_ray;
    te_vec3 camera_world_pos;
    unsigned int count;
    unsigned int i;
    unsigned int child_idx;

    (*out_game_obj) = NULL;
    (*out_game_obj_info) = NULL;

    frustum = camera_get_frustum(camera);

    if (!camera_calc_cursor_world_dir(camera, cursor_pos_rel, camera_world_ray)) {
        return;
    }

    camera_get_world_position(camera, camera_world_pos);

    root_game_objects = world_get_root_game_objects(world, &count);
    if (count == 0) {
        return;
    }

    info.model = NULL;

    for (i = 0; i < count; i++) {
        te_game_object_data* root_game_object = &root_game_objects[i];
        if (root_game_object->info->type != TE_GOT_MODEL) {
            continue;
        }
        model = root_game_object->object;

        if (test_model_hit(&info, frustum, gizmo, camera_world_pos, camera_world_ray, model)) {
            break;
        }

        /* test child models */
        child_idx = 0;
        while (true) {
            te_model* child = model_get_child_model(model, child_idx);
            if (child == NULL) {
                break;
            }

            if (test_model_hit(
                    &info, frustum, gizmo, camera_world_pos, camera_world_ray, child)) {
                break;
            }
            child_idx += 1;
        }
    }

    if (info.model == NULL) {
        free(root_game_objects);
        return;
    }

    (*out_game_obj) = info.model;
    (*out_game_obj_info) = model_get_game_object_info();

    free(root_game_objects);
}
