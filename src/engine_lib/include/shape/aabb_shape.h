#pragma once

#include <stdbool.h>
#include "cglm/mat4.h"
#include "cglm/vec3.h"
#include "math.h"
#include "misc/globals.h"
#include "shape/plane_shape.h"

// Axis-aligned bounding box.
typedef struct te_aabb_shape {
    vec3 center;

    // Half extension (size) of the AABB.
    vec3 extents;
} te_aabb_shape;

// Tells if the AABB is fully behind (inside the negative halfspace of) a plane or not.
static inline bool
aabb_shape_is_behind_plane(te_aabb_shape* aabb, te_plane_shape* plane) {
    // Source: https://github.com/gdbooks/3DCollisions/blob/master/Chapter2/static_aabb_plane.md

    vec3 abs_normal;
    glm_vec3_abs(plane->normal, abs_normal);

    const float proj_radius = glm_vec3_dot(aabb->extents, abs_normal);
    const float dist_to_plane = glm_vec3_dot(plane->normal, aabb->center) - plane->distance;

    return !(-proj_radius <= dist_to_plane);
}

// Transforms AABB from model space to world space.
static inline te_aabb_shape
aabb_shape_convert_to_world(te_aabb_shape* aabb, mat4 world_mat) {
    // We can't just transform AABB to world space (using world matrix) as this would result
    // in OBB (oriented bounding box) because of rotation in world matrix while we need an AABB.

    // Prepare some vec4s.
    vec4 center;
    glm_vec3_copy(aabb->center, center);
    center[3] = 1.0f;

    vec4 forward;
    globals_get_world_forward(forward);
    forward[3] = 0.0f;
    glm_vec4_scale(forward, aabb->extents[2], forward);

    vec4 right;
    globals_get_world_right(right);
    right[3] = 0.0f;
    glm_vec4_scale(right, aabb->extents[0], right);

    vec4 up;
    globals_get_world_up(up);
    up[3] = 0.0f;
    glm_vec4_scale(up, aabb->extents[1], up);

    te_aabb_shape result;
    glm_mat4_mulv(world_mat, center, result.center);

    // Calculate OBB directions in world space
    // (directions are considered to point from OBB's center).
    vec4 obb_forward;
    vec4 obb_right;
    vec4 obb_up;
    glm_mat4_mulv(world_mat, forward, obb_forward);
    glm_mat4_mulv(world_mat, right, obb_right);
    glm_mat4_mulv(world_mat, up, obb_up);

    // If the specified world matrix contained a rotation OBB's directions are no longer aligned
    // with world axes. We need to adjust these OBB directions to be world axis aligned and save them
    // as resulting AABB extents.

    // We can convert scaled OBB directions to AABB extents (directions) by projecting each OBB direction
    // onto world axis.
    result.extents[0] = fabsf(glm_vec4_dot(obb_forward, (vec4){1.0f, 0.0f, 0.0f, 0.0f}))
                        + fabsf(glm_vec4_dot(obb_right, (vec4){1.0f, 0.0f, 0.0f, 0.0f}))
                        + fabsf(glm_vec4_dot(obb_up, (vec4){1.0f, 0.0f, 0.0f, 0.0f}));

    result.extents[1] = fabsf(glm_vec4_dot(obb_forward, (vec4){0.0f, 1.0f, 0.0f, 0.0f}))
                        + fabsf(glm_vec4_dot(obb_right, (vec4){0.0f, 1.0f, 0.0f, 0.0f}))
                        + fabsf(glm_vec4_dot(obb_up, (vec4){0.0f, 1.0f, 0.0f, 0.0f}));

    result.extents[2] = fabsf(glm_vec4_dot(obb_forward, (vec4){0.0f, 0.0f, 1.0f, 0.0f}))
                        + fabsf(glm_vec4_dot(obb_right, (vec4){0.0f, 0.0f, 1.0f, 0.0f}))
                        + fabsf(glm_vec4_dot(obb_up, (vec4){0.0f, 0.0f, 1.0f, 0.0f}));

    return result;
}
