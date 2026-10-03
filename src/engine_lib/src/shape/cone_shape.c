#include <shape/cone_shape.h>

bool
cone_shape_is_behind_plane(te_cone_shape* cone, te_plane_shape* plane) {
    vec3 intermediate;
    vec3 to_bottom;
    vec3 right_part;
    vec3 left_part;
    vec3 point_on_cone;

    /* source: Real-time collision detection, Christer Ericson (2005) */

    glm_cross(plane->normal, cone->direction, intermediate);
    glm_cross(intermediate, cone->direction, intermediate);

    glm_vec3_scale(cone->direction, cone->height, to_bottom);

    glm_vec3_scale(intermediate, -cone->bottom_radius, right_part);

    glm_vec3_add(cone->position, to_bottom, left_part);

    glm_vec3_add(left_part, right_part, point_on_cone);

    return plane_shape_test_point(plane, cone->position)
           && plane_shape_test_point(plane, point_on_cone);
}
