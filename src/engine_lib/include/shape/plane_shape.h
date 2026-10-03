#pragma once

#include <stdbool.h>
#include <cglm/vec3.h>

/* plane represented by a normal and a distance from the origin */
typedef struct te_plane_shape {
    vec3 normal;
    float distance;
} te_plane_shape;

te_plane_shape plane_shape_create(vec3 normal, vec3 position);

/* tells if the point is fully behind (inside the negative halfspace of) a plane or not */
bool plane_shape_test_point(te_plane_shape* shape, vec3 point);

/* returns `false` if intersection was not found */
bool
plane_shape_ray_intersection(te_plane_shape* shape, vec3 ray_origin, vec3 ray, vec3 out_pos);
