#pragma once

#include <stdbool.h>
#include <math/vec3.h>
#include <shape/plane_shape.h>

typedef struct te_cone_shape {
    te_vec3 position;
    float height;
    te_vec3 direction;
    float bottom_radius;
} te_cone_shape;

/* tells if the cone is fully behind (inside the negative halfspace of) a plane or not */
bool cone_shape_is_behind_plane(te_cone_shape* cone, te_plane_shape* plane);
