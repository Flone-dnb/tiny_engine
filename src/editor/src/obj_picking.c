#include "obj_picking.h"

#include <world.h>
#include <game/camera.h>
#include <game/model.h>
#include <render/model_renderer.h>
#include <shape/frustum_shape.h>
#include <gizmo.h>

void*
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
    te_model** models = world_get_models(world, &count);

    struct closest_model_info {
        te_model* model;
        te_aabb_shape aabb_world;
        float bb_size;
        float distance;
    };
    struct closest_model_info info;
    info.model = NULL;

    unsigned int handle = 0xFFFFFFFF;
    for (unsigned int i = 0; i < count; i++) {
        handle = prv_model_get_render_data_handle(models[i]);
        if (handle == 0xFFFFFFFF) {
            continue;
        }

        te_model_renderer* renderer = prv_model_get_model_renderer(models[i]);
        if (renderer == NULL) {
            continue;
        }

        te_model_render_data* data = model_renderer_get_render_data_tmp(renderer, handle);
        if (!frustum_shape_is_aabb_inside(frustum, &data->aabb_world)) {
            continue;
        }

        float distance;
        if (!aabb_shape_intersect_ray(
                &data->aabb_world, camera_world_pos, camera_world_ray, &distance)) {
            continue;
        }

        if (gizmo != NULL) {
            if (models[i] == gizmo_get_model_x(gizmo) || models[i] == gizmo_get_model_y(gizmo)
                || models[i] == gizmo_get_model_z(gizmo)) {
                // Always prioritize gizmo.
                info.model = models[i];
                break;
            }
        }

        const float bb_size = data->aabb_world.extents[0] * 2.0f * data->aabb_world.extents[1]
                              * 2.0f * data->aabb_world.extents[2] * 2.0f;

        // TODO: for now just do a bunch of simple tests (no ray-triangle intersection
        // because we don't store the geometry on the CPU).
        if (info.model != NULL) {
            if (!aabb_shape_intersect(&data->aabb_world, &info.aabb_world)) {
                if (distance >= info.distance) {
                    continue;
                }
            } else {
                if (bb_size >= info.bb_size) {
                    continue;
                }
            }
        }

        info.model = models[i];
        info.aabb_world = data->aabb_world;
        info.bb_size = bb_size;
        info.distance = distance;
    }

    free(models);

    return info.model;
}