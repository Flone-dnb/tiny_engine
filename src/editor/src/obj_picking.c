#include "obj_picking.h"

#include "world.h"
#include "game/camera.h"
#include "game/model.h"
#include "render/model_renderer.h"
#include "shape/frustum_shape.h"

void* obj_picking_find_obj_under_cursor(vec2 cursor_pos_rel, te_camera* camera, te_world* world) {
    te_frustum_shape* frustum = camera_get_frustum(camera);
    vec3 camera_world_pos;
    camera_get_position(camera, camera_world_pos);

    // Convert mouse pos to NDC [-1; 1] space.
    vec2 ndc;
    glm_vec2_mul(cursor_pos_rel, (vec2){2.0f, 2.0f}, ndc);
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

    // Get direction from camera pos.
    glm_vec3_sub(camera_ray, camera_world_pos, camera_ray);
    glm_vec3_normalize(camera_ray);

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
                &data->aabb_world, camera_world_pos, camera_ray, &distance)) {
            continue;
        }

        // TODO: for now just do a bunch of simple tests (no ray-triangle intersection
        // because we don't store the geometry on the CPU).
        const float bb_size = data->aabb_world.extents[0] * 2.0f * data->aabb_world.extents[1]
                              * 2.0f * data->aabb_world.extents[2] * 2.0f;
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