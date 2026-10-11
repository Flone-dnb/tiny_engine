#include <math/math_funcs.h>

#include <stdlib.h>
#include <math.h>
#include <io/log.h>
#include <misc/globals.h>

#define MATH_PI 3.14159265f

float
math_pi(void) {
    return MATH_PI;
}

float
math_abs(float value) {
    return (float)fabs(value);
}

float
math_max(float a, float b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

float
math_min(float a, float b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

float
math_clamp(float value, float min, float max) {
    if (value < min) {
        return min;
    } else if (value > max) {
        return max;
    } else {
        return value;
    }
}

float
math_rad(float deg) {
    return deg * MATH_PI / 180.0f;
}

float
math_deg(float rad) {
    return rad * 180.0f / MATH_PI;
}

float
math_tan(float value) {
    return (float)tan(value);
}

float
math_asin(float value) {
    return (float)asin(value);
}

float
math_atan2(float y, float x) {
    return (float)atan2(y, x);
}

float
math_sqrt(float value) {
    return (float)sqrt(value);
}

float
math_cos(float value) {
    return (float)cos(value);
}

float
math_sin(float value) {
    return (float)sin(value);
}

float
math_round(float value) {
    if (value >= 0.0f) {
        return (float)(int)(value + 0.5f);
    } else {
        return (float)(int)(value - 0.5f);
    }
}

float
math_ceil(float value) {
    return (float)ceil(value);
}

float
math_mod(float a, float b) {
    return (float)fmod(a, b);
}

float
math_smoothstep(float edge0, float edge1, float x) {
    x = math_clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

float
math_lerp(float from, float to, float t) {
    return from + t * (to - from);
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
    if (math_abs(dir[0] + dir[1] + dir[2]) < 0.01f) {
        vec3_zero(out);
        return;
    }

#if defined(DEBUG)
    {
        /* make sure we are given a normalized direction */
        float len = vec3_len(dir);
        if (len >= 1.01f || len <= 0.99f) {
            log_error(
                __FILE__, __LINE__,
                "the specified direction vector should have been normalized");
            abort();
        }
    }
#endif

    out[0] = math_deg((float)-asin(dir[1]));
    out[1] = math_deg((float)atan2(dir[0], dir[2]));
    out[2] = 0.0f;

    /* check for NaNs */
    if (out[0] != out[0]) {
        out[0] = 0.0f;
    }
    if (out[1] != out[1]) {
        out[1] = 0.0f;
    }
}

void
math_make_rotation_mat(te_vec3 rotation_deg, te_mat4 out) {
    te_mat4 z_rot;
    te_mat4 y_rot;
    te_mat4 x_rot;
    te_vec3 z;
    te_vec3 y;
    te_vec3 x;

    vec3_zero(z);
    vec3_zero(y);
    vec3_zero(x);

    z[2] = 1.0f;
    y[1] = 1.0f;
    x[0] = 1.0f;

    mat4_make_rotate_from_norm_axis(z_rot, math_rad(rotation_deg[2]), z);
    mat4_make_rotate_from_norm_axis(y_rot, math_rad(rotation_deg[1]), y);
    mat4_make_rotate_from_norm_axis(x_rot, math_rad(rotation_deg[0]), x);

    mat4_mul(y_rot, z_rot, out);
    mat4_mul(out, x_rot, out);
}

void
math_convert_rot_to_norm_dir(te_vec3 rot, te_vec3 out) {
    te_mat4 rot_mat;
    te_vec4 forward;
    te_vec4 result;

    math_make_rotation_mat(rot, rot_mat);

    globals_get_world_forward(forward);
    forward[3] = 0.0f;

    mat4_mulv(rot_mat, forward, result);

    vec3_copy(result, out);
}
