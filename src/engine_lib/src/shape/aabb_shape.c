#include <shape/aabb_shape.h>

#include <math/math_funcs.h>
#include <misc/globals.h>

bool
aabb_shape_is_behind_plane(te_aabb_shape* aabb, te_plane_shape* plane) {
    te_vec3 abs_normal;
    float proj_radius;
    float dist_to_plane;

    /* source: https://github.com/gdbooks/3DCollisions/blob/master/Chapter2/static_aabb_plane.md */

    abs_normal[0] = math_abs(plane->normal[0]);
    abs_normal[1] = math_abs(plane->normal[1]);
    abs_normal[2] = math_abs(plane->normal[2]);

    proj_radius = vec3_dot(aabb->extents, abs_normal);
    dist_to_plane = vec3_dot(plane->normal, aabb->center) - plane->distance;

    return !(-proj_radius <= dist_to_plane);
}

bool
aabb_shape_intersect(te_aabb_shape* a, te_aabb_shape* b) {
    return a->center[0] - a->extents[0] <= b->center[0] + b->extents[0]
           && a->center[0] + a->extents[0] >= b->center[0] - b->extents[0]
           && a->center[1] - a->extents[1] <= b->center[1] + b->extents[1]
           && a->center[1] + a->extents[1] >= b->center[1] - b->extents[1]
           && a->center[2] - a->extents[2] <= b->center[2] + b->extents[2]
           && a->center[2] + a->extents[2] >= b->center[2] - b->extents[2];
}

bool
aabb_shape_intersect_ray(
    te_aabb_shape* aabb, te_vec3 ray_origin, te_vec3 ray_dir, float* hit_dist_along_ray) {
    te_vec3 min;
    te_vec3 max;
    te_vec3 tmin;
    te_vec3 tmax;
    te_vec3 t1;
    te_vec3 t2;
    float t_near, t_far;

    vec3_sub(aabb->center, aabb->extents, min);
    vec3_add(aabb->center, aabb->extents, max);

    vec3_sub(min, ray_origin, tmin);
    vec3_div(tmin, ray_dir, tmin);

    vec3_sub(max, ray_origin, tmax);
    vec3_div(tmax, ray_dir, tmax);

    t1[0] = math_min(tmin[0], tmax[0]);
    t1[1] = math_min(tmin[1], tmax[1]);
    t1[2] = math_min(tmin[2], tmax[2]);

    t2[0] = math_max(tmin[0], tmax[0]);
    t2[1] = math_max(tmin[1], tmax[1]);
    t2[2] = math_max(tmin[2], tmax[2]);

    t_near = math_max(math_max(t1[0], t1[1]), t1[2]);
    t_far = math_min(math_min(t2[0], t2[1]), t2[2]);

    if (t_near > t_far) {
        return false;
    }

    (*hit_dist_along_ray) = t_near;
    return true;
}

te_aabb_shape
aabb_shape_convert_to_world(te_aabb_shape* aabb, te_mat4 world_mat) {
    te_aabb_shape result;
    te_vec4 center;
    te_vec4 forward;
    te_vec4 right;
    te_vec4 up;
    te_vec4 obb_forward;
    te_vec4 obb_right;
    te_vec4 obb_up;

    /* we can't just transform AABB to world space (using world matrix) as this would result
     * in OBB (oriented bounding box) because of rotation in world matrix while we need an AABB */

    /* prepare some vec4s */

    /* center */
    vec3_copy(aabb->center, center);
    center[3] = 1.0f;

    /* forward */
    globals_get_world_forward(forward);
    forward[3] = 0.0f;
    vec4_muls(forward, aabb->extents[2], forward);

    /* right */
    globals_get_world_right(right);
    right[3] = 0.0f;
    vec4_muls(right, aabb->extents[0], right);

    /* up */
    globals_get_world_up(up);
    up[3] = 0.0f;
    vec4_muls(up, aabb->extents[1], up);

    mat4_mulv(world_mat, center, center);

    vec3_copy(center, result.center);

    /* calculate OBB directions in world space
     * (directions are considered to point from OBB's center) */
    mat4_mulv(world_mat, forward, obb_forward);
    mat4_mulv(world_mat, right, obb_right);
    mat4_mulv(world_mat, up, obb_up);

    /* if the specified world matrix contained a rotation OBB's directions are no longer aligned
     * with world axes, we need to adjust these OBB directions to be world axis aligned and save them
     * as resulting AABB extents */

    /* we can convert scaled OBB directions to AABB extents (directions) by projecting each
     * OBB direction onto world axis */
    vec4_set(1.0f, 0.0f, 0.0f, 0.0f, forward);
    result.extents[0] = math_abs(vec4_dot(obb_forward, forward))
                        + math_abs(vec4_dot(obb_right, forward))
                        + math_abs(vec4_dot(obb_up, forward));

    vec4_set(0.0f, 1.0f, 0.0f, 0.0f, forward);
    result.extents[1] = math_abs(vec4_dot(obb_forward, forward))
                        + math_abs(vec4_dot(obb_right, forward))
                        + math_abs(vec4_dot(obb_up, forward));

    vec4_set(0.0f, 0.0f, 1.0f, 0.0f, forward);
    result.extents[2] = math_abs(vec4_dot(obb_forward, forward))
                        + math_abs(vec4_dot(obb_right, forward))
                        + math_abs(vec4_dot(obb_up, forward));

    return result;
}
