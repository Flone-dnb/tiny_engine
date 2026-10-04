#pragma once

#include <stdbool.h>
#include <cglm/mat4.h>
#include <math/vec3.h>
#include <shape/plane_shape.h>

/* axis-aligned bounding box */
typedef struct te_aabb_shape {
    te_vec3 center;

    /* half extension (size) of the AABB */
    te_vec3 extents;
} te_aabb_shape;

/* tells if the AABB is fully behind (inside the negative halfspace of) a plane or not */
bool aabb_shape_is_behind_plane(te_aabb_shape* aabb, te_plane_shape* plane);

/* returns `true` if AABBs are intersecting */
bool aabb_shape_intersect(te_aabb_shape* a, te_aabb_shape* b);

/* returns `true` if the ray intersects AABB
 * also (if hit is found) writes distance along the ray until the hit position */
bool aabb_shape_intersect_ray(
    te_aabb_shape* aabb, te_vec3 ray_origin, te_vec3 ray_dir, float* hit_dist_along_ray);

/* transforms AABB from model space to world space */
te_aabb_shape aabb_shape_convert_to_world(te_aabb_shape* aabb, mat4 world_mat);
