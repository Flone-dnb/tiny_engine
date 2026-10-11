#include <shape/cone_shape.h>

bool
cone_shape_is_behind_plane(te_cone_shape* cone, te_plane_shape* plane) {
    te_vec3 intermediate;
    te_vec3 to_bottom;
    te_vec3 right_part;
    te_vec3 left_part;
    te_vec3 point_on_cone;

    /* source: Real-time collision detection, Christer Ericson (2005) */

    vec3_cross(plane->normal, cone->direction, intermediate);
    vec3_cross(intermediate, cone->direction, intermediate);

    vec3_muls(cone->direction, cone->height, to_bottom);

    vec3_muls(intermediate, -cone->bottom_radius, right_part);

    vec3_add(cone->position, to_bottom, left_part);

    vec3_add(left_part, right_part, point_on_cone);

    return plane_shape_test_point(plane, cone->position)
           && plane_shape_test_point(plane, point_on_cone);
}
