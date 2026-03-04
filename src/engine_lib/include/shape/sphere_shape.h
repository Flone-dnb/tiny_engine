#pragma once

#include <cglm/vec3.h>
#include <shape/plane_shape.h>

typedef struct te_sphere_shape {
    vec3 center;
    float radius;
} te_sphere_shape;

// Tells if the sphere is fully behind (inside the negative halfspace of) a plane or not.
static inline bool
sphere_shape_is_behind_plane(te_sphere_shape* sphere, te_plane_shape* plane) {
    // Source: Real-time collision detection, Christer Ericson (2005).
    return glm_dot(plane->normal, sphere->center) - plane->distance < -sphere->radius;
}
