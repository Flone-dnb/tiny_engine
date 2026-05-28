#include "obj_picking.h"

#include <world.h>
#include <game/camera.h>
#include <game/model.h>
#include <game/game_object_info.h>
#include <render/model_renderer.h>
#include <shape/frustum_shape.h>
#include <gizmo.h>

typedef struct{
    te_model* model;
    te_aabb_shape aabb_world;
    float bb_size;
    float distance;
} closest_model_info;

// Updates closest model info if hit.
// Returns `true` if hit gizmo and need to quit (not check other models).
static bool
test_model_hit(
    closest_model_info* closest_info, te_frustum_shape* frustum, te_gizmo* gizmo,
    vec3 camera_world_pos, vec3 camera_world_ray, te_model* model) {
    const unsigned int handle = prv_model_get_render_data_handle(model);
    if (handle == 0xFFFFFFFF) {
        return false;
    }

    te_model_renderer* renderer = prv_model_get_model_renderer(model);
    if (renderer == NULL) {
        return false;
    }

    te_model_render_data* data = model_renderer_get_render_data_tmp(renderer, handle);
    if (!frustum_shape_is_aabb_inside(frustum, &data->aabb_world)) {
        return false;
    }

    float distance;
    if (!aabb_shape_intersect_ray(
            &data->aabb_world, camera_world_pos, camera_world_ray, &distance)) {
        return false;
    }

    if (gizmo != NULL) {
        if (model == gizmo_get_model_x(gizmo) || model == gizmo_get_model_y(gizmo)
            || model == gizmo_get_model_z(gizmo)) {
            // Always prioritize gizmo.
            closest_info->model = model;
            return true;
        }
    }

    const float bb_size = data->aabb_world.extents[0] * 2.0f * data->aabb_world.extents[1]
                          * 2.0f * data->aabb_world.extents[2] * 2.0f;

    // TODO: for now just do a bunch of simple tests (no ray-triangle intersection
    // because we don't store the geometry on the CPU).
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

te_game_object_info*
obj_picking_find_obj_under_cursor(
    vec2 cursor_pos_rel, te_camera* camera, te_world* world, te_gizmo* gizmo) {
    te_frustum_shape* frustum = camera_get_frustum(camera);

    vec3 camera_world_ray;
    if (!camera_calc_cursor_world_dir(camera, cursor_pos_rel, camera_world_ray)) {
        return NULL;
    }

    vec3 camera_world_pos;
    camera_get_world_position(camera, camera_world_pos);

    unsigned int count;
    te_game_object_info** root_game_objects = world_get_root_game_objects(world, &count);
    if (count == 0) {
        return NULL;
    }

    closest_model_info info;
    info.model = NULL;

    for (unsigned int i = 0; i < count; i++) {
        te_game_object_info* root_game_object = root_game_objects[i];
        if (root_game_object->type != TE_GOT_MODEL) {
            continue;
        }
        te_model* model = root_game_object->game_object;

        if (test_model_hit(&info, frustum, gizmo, camera_world_pos, camera_world_ray, model)) {
            break;
        }

        te_model* child = model_get_child_model(model);
        if (child != NULL) {
            if (test_model_hit(
                    &info, frustum, gizmo, camera_world_pos, camera_world_ray, child)) {
                break;
            }
        }
    }

    if (info.model == NULL) {
        free(root_game_objects);
        return NULL;
    }

    te_game_object_info* picked_game_object = model_get_game_object_info(info.model);

    free(root_game_objects);
    return picked_game_object;
}