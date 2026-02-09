#pragma once

#include <stdbool.h>
#include "cglm/vec3.h"
#include "shape/plane_shape.h"

typedef struct te_cone_shape {
    vec3 position;
    float height;
    vec3 direction;
    float bottom_radius;
} te_cone_shape;

// Tells if the cone is fully behind (inside the negative halfspace of) a plane or not.
static inline bool
cone_shape_is_behind_plane(te_cone_shape* cone, te_plane_shape* plane) {
    // Source: Real-time collision detection, Christer Ericson (2005).

    vec3 intermediate;
    glm_cross(plane->normal, cone->direction, intermediate);
    glm_cross(intermediate, cone->direction, intermediate);

    vec3 to_bottom;
    glm_vec3_scale(cone->direction, cone->height, to_bottom);

    vec3 right_part;
    glm_vec3_scale(intermediate, -cone->bottom_radius, right_part);

    vec3 left_part;
    glm_vec3_add(cone->position, to_bottom, left_part);

    vec3 point_on_cone;
    glm_vec3_add(left_part, right_part, point_on_cone);

    return plane_shape_test_point(plane, cone->position) && plane_shape_test_point(plane, point_on_cone);
}
