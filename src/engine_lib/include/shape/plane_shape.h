#pragma once

#include <stdbool.h>
#include "cglm/vec3.h"

// Plane represented by a normal and a distance from the origin.
typedef struct te_plane_shape {
    vec3 normal;
    float distance;
} te_plane_shape;

static inline te_plane_shape
plane_shape_create(vec3 normal, vec3 location) {
    te_plane_shape plane;
    glm_vec3_copy(normal, plane.normal);
    plane.distance = glm_vec3_dot(normal, location);

    return plane;
}

// Tells if the point is fully behind (inside the negative halfspace of) a plane or not.
static inline bool
plane_shape_test_point(te_plane_shape* shape, vec3 point) {
    // Source: Real-time collision detection, Christer Ericson (2005).
    return glm_dot(shape->normal, point) - shape->distance < 0.0f;
}
