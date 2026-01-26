#pragma once

#include "cglm/cglm.h"

/**
 * Creates a new rotation matrix.
 *
 * @param rotation_deg Rotation in degrees.
 * @param out          Resulting rotation matrix.
 */
void
math_make_rotation_mat(vec3 rotation_deg, mat4 out) {
    vec3 z = GLM_VEC3_ZERO;
    vec3 y = GLM_VEC3_ZERO;
    vec3 x = GLM_VEC3_ZERO;
    z[2] = 1.0f;
    y[1] = 1.0f;
    x[0] = 1.0f;

    mat4 z_rot;
    glm_rotate_make(z_rot, glm_rad(rotation_deg[2]), z);

    mat4 y_rot;
    glm_rotate_make(y_rot, glm_rad(rotation_deg[1]), y);

    mat4 x_rot;
    glm_rotate_make(x_rot, glm_rad(rotation_deg[0]), x);

    // The order of rotations is: Z first, then Y, then X.
    glm_mat4_mul(y_rot, z_rot, out);
    glm_mat4_mul(x_rot, out, out);
}
