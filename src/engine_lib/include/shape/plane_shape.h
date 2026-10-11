#pragma once

#include <stdbool.h>
#include <math/vec3.h>

/* plane represented by a normal and a distance from the origin */
typedef struct te_plane_shape {
    te_vec3 normal;
    float distance;
} te_plane_shape;

te_plane_shape plane_shape_create(te_vec3 normal, te_vec3 position);

/* tells if the point is fully behind (inside the negative halfspace of) a plane or not */
bool plane_shape_test_point(te_plane_shape* shape, te_vec3 point);

/* returns `false` if intersection was not found */
bool
plane_shape_ray_intersection(te_plane_shape* shape, te_vec3 ray_origin, te_vec3 ray, te_vec3 out_pos);
