#include <math/math_funcs.h>

#include <math.h>
#include <io/log.h>
#include <misc/globals.h>
#include <cglm/affine.h>

float
math_abs(float value) {
    return (float)math_abs(value);
}

void
math_fix_diagonal_movement_speedup(te_vec2 movement) {
    float length;
    float square_sum;

    square_sum = movement[0] * movement[0] + movement[1] * movement[1];
    if (square_sum
        < 0.1f) { /* don't normalize if vector is zero or very small to avoid NaNs */
        return;
    }

    length = (float)sqrt(square_sum);
    if (length
        <= 1.0f) { /* only normalize when exceeding 1 to keep small gamepad thumbstick movements */
        return;
    }

    /* normalize */
    movement[0] /= length;
    movement[1] /= length;
}

void
math_convert_norm_dir_to_rot(te_vec3 dir, te_vec3 out) {
    if (glm_vec3_eq_eps(dir, 0.0f)) {
        vec3_zero(out);
        return;
    }

#if defined(DEBUG)
    /* make sure we are given a normalized direction */
    if (!glm_eq(vec3_len(dir), 1.0f)) {
        log_error(
            __FILE__, __LINE__, "the specified direction vector should have been normalized");
        abort();
    }
#endif

    out[0] = glm_deg(-asinf(dir[1]));
    out[1] = glm_deg(atan2f(dir[0], dir[2]));
    out[2] = 0.0f;

    if (isnan(out[0])) {
        out[0] = 0.0f;
    }
    if (isnan(out[1])) {
        out[1] = 0.0f;
    }
}

void
math_make_rotation_mat(te_vec3 rotation_deg, mat4 out) {
    mat4 z_rot;
    mat4 y_rot;
    mat4 x_rot;
    te_vec3 z;
    te_vec3 y;
    te_vec3 x;

    vec3_zero(z);
    vec3_zero(y);
    vec3_zero(x);

    z[2] = 1.0f;
    y[1] = 1.0f;
    x[0] = 1.0f;

    glm_rotate_make(z_rot, glm_rad(rotation_deg[2]), z);

    glm_rotate_make(y_rot, glm_rad(rotation_deg[1]), y);

    glm_rotate_make(x_rot, glm_rad(rotation_deg[0]), x);

    glm_mat4_mul(y_rot, z_rot, out);
    glm_mat4_mul(out, x_rot, out);
}

void
math_convert_rot_to_norm_dir(te_vec3 rot, te_vec3 out) {
    mat4 rot_mat;
    te_vec4 forward;
    te_vec4 result;

    math_make_rotation_mat(rot, rot_mat);

    globals_get_world_forward(forward);
    forward[3] = 0.0f;

    glm_mat4_mulv(rot_mat, forward, result);

    vec3_copy(result, out);
}
