#include <shape/plane_shape.h>

te_plane_shape
plane_shape_create(vec3 normal, vec3 position) {
    te_plane_shape plane;
    glm_vec3_copy(normal, plane.normal);
    plane.distance = glm_vec3_dot(normal, position);

    return plane;
}

bool
plane_shape_test_point(te_plane_shape* shape, vec3 point) {
    /* source: Real-time collision detection, Christer Ericson (2005) */
    return glm_vec3_dot(shape->normal, point) - shape->distance < 0.0f;
}

bool
plane_shape_ray_intersection(te_plane_shape* shape, vec3 ray_origin, vec3 ray, vec3 out_pos) {
    vec3 temp;
    float t;

    float d = glm_vec3_dot(shape->normal, ray);
    if (fabs(d) < 0.0001f) {
        return false;
    }

    glm_vec3_scale(shape->normal, shape->distance, temp);

    glm_vec3_sub(temp, ray_origin, temp);
    t = glm_vec3_dot(temp, shape->normal) / d;
    if (t < 0.0f) {
        return false;
    }

    glm_vec3_copy(ray, temp);
    glm_vec3_scale(temp, t, temp);

    glm_vec3_copy(ray_origin, out_pos);
    glm_vec3_add(out_pos, temp, out_pos);
    return true;
}
