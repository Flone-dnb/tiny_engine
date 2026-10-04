#pragma once

#include <math/vec3.h>
#include <shape/plane_shape.h>

typedef struct te_sphere_shape {
    te_vec3 center;
    float radius;
} te_sphere_shape;

/* tells if the sphere is fully behind (inside the negative halfspace of) a plane or not */
bool sphere_shape_is_behind_plane(te_sphere_shape* sphere, te_plane_shape* plane);
