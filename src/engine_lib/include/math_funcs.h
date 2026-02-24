#pragma once

#include "cglm/affine.h"
#include "cglm/mat4.h"
#include "cglm/vec3.h"
#include "misc/error.h"
#include "misc/globals.h"

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

// Converts a normalized direction vector to rotation angles.
static inline void
math_convert_norm_dir_to_rot(vec3 dir, vec3 out) {
    if (glm_vec3_eq_eps(dir, 0.0f)) {
        glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, out);
    }

#if defined(DEBUG)
    // Make sure we are given a normalized direction.
    if (!glm_eq(glm_vec3_norm(dir), 1.0f)) {
        show_error_and_abort("the specified direction vector should have been normalized");
    }
#endif

    out[0] = glm_deg(asinf(dir[1]));
    out[1] = glm_deg(atan2f(dir[0], dir[2]));
    out[2] = 0.0f;

    if (isnan(out[0])) {
        out[0] = 0.0f;
    }
    if (isnan(out[1])) {
        out[1] = 0.0f;
    }
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

// Converts rotation angles to a normalized direction vector.
static inline void
math_convert_rot_to_norm_dir(vec3 rot, vec3 out) {
    mat4 rot_mat;
    math_make_rotation_mat(rot, rot_mat);

    vec4 forward;
    globals_get_world_forward(forward);
    forward[3] = 0.0f;

    vec4 result;
    glm_mat4_mulv(rot_mat, forward, result);

    glm_vec3_copy(result, out);
}
