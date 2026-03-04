#pragma once

#include <cglm/vec3.h>
#include <shape/aabb_shape.h>
#include <shape/cone_shape.h>
#include <shape/plane_shape.h>
#include <shape/sphere_shape.h>

typedef struct te_frustum_shape {
    te_plane_shape top;
    te_plane_shape bottom;
    te_plane_shape right;
    te_plane_shape left;
    te_plane_shape near;
    te_plane_shape far;
} te_frustum_shape;

static inline te_frustum_shape
frustum_shape_create(vec3 camera_pos, vec3 forward, vec3 up, float near_clip, float far_clip,
                     float vertical_fov, float aspect_ratio) {
    const float tan_half_fov = tanf(0.5f * vertical_fov);
    const float far_half_height = far_clip * tan_half_fov;
    const float far_half_width = far_half_height * aspect_ratio;

    vec3 right;
    glm_vec3_cross(forward, up, right);
    glm_vec3_normalize(right);

    vec3 far_normal;
    glm_vec3_copy(forward, far_normal);
    glm_vec3_negate(far_normal);

    vec3 to_near;
    glm_vec3_scale(forward, near_clip, to_near);

    vec3 to_far;
    glm_vec3_scale(forward, far_clip, to_far);

    vec3 near_pos;
    glm_vec3_add(camera_pos, to_near, near_pos);

    vec3 far_pos;
    glm_vec3_add(camera_pos, to_far, far_pos);

    te_frustum_shape frustum;
    frustum.near = plane_shape_create(forward, near_pos);
    frustum.far = plane_shape_create(far_normal, far_pos);

    vec3 right_scaled;
    glm_vec3_scale(right, far_half_width, right_scaled);
    vec3 neg_right_scaled;
    glm_vec3_copy(right_scaled, neg_right_scaled);
    glm_vec3_negate(neg_right_scaled);

    vec3 temp;
    glm_vec3_add(to_far, right_scaled, temp);
    glm_vec3_cross(up, temp, temp);
    glm_vec3_normalize(temp);
    frustum.right = plane_shape_create(temp, camera_pos);

    glm_vec3_add(to_far, neg_right_scaled, temp);
    glm_vec3_cross(temp, up, temp);
    glm_vec3_normalize(temp);
    frustum.left = plane_shape_create(temp, camera_pos);

    vec3 up_scaled;
    glm_vec3_scale(up, far_half_height, up_scaled);
    vec3 neg_up_scaled;
    glm_vec3_copy(up_scaled, neg_up_scaled);
    glm_vec3_negate(neg_up_scaled);

    glm_vec3_add(to_far, up_scaled, temp);
    glm_vec3_cross(temp, right, temp);
    glm_vec3_normalize(temp);
    frustum.top = plane_shape_create(temp, camera_pos);

    glm_vec3_add(to_far, neg_up_scaled, temp);
    glm_vec3_cross(right, temp, temp);
    glm_vec3_normalize(temp);
    frustum.bottom = plane_shape_create(temp, camera_pos);

    return frustum;
}

// Tests if the specified axis-aligned bounding box is inside of the frustum or intersects it.
static inline bool
frustum_shape_is_aabb_inside(te_frustum_shape* frustum, te_aabb_shape* aabb) {
    return !aabb_shape_is_behind_plane(aabb, &frustum->left)
           && !aabb_shape_is_behind_plane(aabb, &frustum->right)
           && !aabb_shape_is_behind_plane(aabb, &frustum->top)
           && !aabb_shape_is_behind_plane(aabb, &frustum->bottom)
           && !aabb_shape_is_behind_plane(aabb, &frustum->near)
           && !aabb_shape_is_behind_plane(aabb, &frustum->far);
}

// Tests if the specified sphere is inside of the frustum or intersects it.
static inline bool
frustum_shape_is_sphere_inside(te_frustum_shape* frustum, te_sphere_shape* sphere) {
    return !sphere_shape_is_behind_plane(sphere, &frustum->left)
           && !sphere_shape_is_behind_plane(sphere, &frustum->right)
           && !sphere_shape_is_behind_plane(sphere, &frustum->top)
           && !sphere_shape_is_behind_plane(sphere, &frustum->bottom)
           && !sphere_shape_is_behind_plane(sphere, &frustum->near)
           && !sphere_shape_is_behind_plane(sphere, &frustum->far);
}

// Tests if the specified cone is inside of the frustum or intersects it.
static inline bool
frustum_shape_is_cone_inside(te_frustum_shape* frustum, te_cone_shape* cone) {
    return !cone_shape_is_behind_plane(cone, &frustum->left)
           && !cone_shape_is_behind_plane(cone, &frustum->right)
           && !cone_shape_is_behind_plane(cone, &frustum->top)
           && !cone_shape_is_behind_plane(cone, &frustum->bottom)
           && !cone_shape_is_behind_plane(cone, &frustum->near)
           && !cone_shape_is_behind_plane(cone, &frustum->far);
}
