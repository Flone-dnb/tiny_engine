#pragma once

#include "cglm/cglm.h"

// Used to fix a common problem where diagonal movement has ~1.4 speed instead of 1.
static inline void
math_fix_diagonal_movement_speedup(vec2 movement) {
    const float square_sum = movement[0] * movement[0] + movement[1] * movement[1];
    if (square_sum < 0.1f) { // don't normalize if vector is zero or very small to avoid NaNs
        return;
    }

    const float length = sqrtf(square_sum);
    if (length <= 1.0f) { // only normalize when exceeding 1 to keep small gamepad thumbstick movements
        return;
    }

    // normalize
    movement[0] /= length;
    movement[1] /= length;
}

// Creates a new rotation matrix from a rotation (in degrees).
static inline void
math_make_rotation_mat(vec3 rotation_deg, mat4 out) {
    vec3 z;
    vec3 y;
    vec3 x;
    glm_vec3_make((vec3){0.0f, 0.0f, 1.0f}, z);
    glm_vec3_make((vec3){0.0f, 1.0f, 0.0f}, y);
    glm_vec3_make((vec3){1.0f, 0.0f, 0.0f}, x);

    mat4 z_rot;
    glm_rotate_make(z_rot, glm_rad(rotation_deg[2]), z);

    mat4 y_rot;
    glm_rotate_make(y_rot, glm_rad(rotation_deg[1]), y);

    mat4 x_rot;
    glm_rotate_make(x_rot, glm_rad(rotation_deg[0]), x);

    glm_mat4_mul(y_rot, z_rot, out);
    glm_mat4_mul(out, x_rot, out);
}
