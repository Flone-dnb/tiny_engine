#include <shape/plane_shape.h>

#include <math/math_funcs.h>

te_plane_shape
plane_shape_create(te_vec3 normal, te_vec3 position) {
    te_plane_shape plane;
    vec3_copy(normal, plane.normal);
    plane.distance = vec3_dot(normal, position);

    return plane;
}

bool
plane_shape_test_point(te_plane_shape* shape, te_vec3 point) {
    /* source: Real-time collision detection, Christer Ericson (2005) */
    return vec3_dot(shape->normal, point) - shape->distance < 0.0f;
}

bool
plane_shape_ray_intersection(
    te_plane_shape* shape, te_vec3 ray_origin, te_vec3 ray, te_vec3 out_pos) {
    te_vec3 temp;
    float t;

    float d = vec3_dot(shape->normal, ray);
    if (math_abs(d) < 0.0001f) {
        return false;
    }

    vec3_muls(shape->normal, shape->distance, temp);

    vec3_sub(temp, ray_origin, temp);
    t = vec3_dot(temp, shape->normal) / d;
    if (t < 0.0f) {
        return false;
    }

    vec3_copy(ray, temp);
    vec3_muls(temp, t, temp);

    vec3_copy(ray_origin, out_pos);
    vec3_add(out_pos, temp, out_pos);
    return true;
}
