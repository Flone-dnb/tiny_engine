#include <shape/sphere_shape.h>

bool
sphere_shape_is_behind_plane(te_sphere_shape* sphere, te_plane_shape* plane) {
    /* source: Real-time collision detection, Christer Ericson (2005) */
    return vec3_dot(plane->normal, sphere->center) - plane->distance < -sphere->radius;
}
