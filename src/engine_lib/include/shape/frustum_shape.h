#pragma once

#include <cglm/vec3.h>
#include <shape/plane_shape.h>

struct te_aabb_shape;
struct te_sphere_shape;
struct te_cone_shape;

typedef struct te_frustum_shape {
    te_plane_shape top;
    te_plane_shape bottom;
    te_plane_shape right;
    te_plane_shape left;
    te_plane_shape near;
    te_plane_shape far;
} te_frustum_shape;

te_frustum_shape frustum_shape_create(
    vec3 camera_pos, vec3 forward, vec3 up, float near_clip, float far_clip,
    float vertical_fov, float aspect_ratio);

/* tests if the specified axis-aligned bounding box is inside of the frustum or intersects it */
bool frustum_shape_is_aabb_inside(te_frustum_shape* frustum, struct te_aabb_shape* aabb);

/* tests if the specified sphere is inside of the frustum or intersects it */
bool frustum_shape_is_sphere_inside(te_frustum_shape* frustum, struct te_sphere_shape* sphere);

/* tests if the specified cone is inside of the frustum or intersects it */
bool frustum_shape_is_cone_inside(te_frustum_shape* frustum, struct te_cone_shape* cone);
