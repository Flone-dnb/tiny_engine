#include <math/mat4.h>

#include <stdlib.h>
#include <math/math_funcs.h>
#if defined(DEBUG)
#include <io/log.h>
#endif

void
mat4_identity(te_mat4 mat) {
    mat[0][0] = 1.0f;
    mat[0][1] = 0.0f;
    mat[0][2] = 0.0f;
    mat[0][3] = 0.0f;

    mat[1][0] = 0.0f;
    mat[1][1] = 1.0f;
    mat[1][2] = 0.0f;
    mat[1][3] = 0.0f;

    mat[2][0] = 0.0f;
    mat[2][1] = 0.0f;
    mat[2][2] = 1.0f;
    mat[2][3] = 0.0f;

    mat[3][0] = 0.0f;
    mat[3][1] = 0.0f;
    mat[3][2] = 0.0f;
    mat[3][3] = 1.0f;
}

void
mat4_copy(te_mat4 from, te_mat4 to) {
    to[0][0] = from[0][0];
    to[0][1] = from[0][1];
    to[0][2] = from[0][2];
    to[0][3] = from[0][3];

    to[1][0] = from[1][0];
    to[1][1] = from[1][1];
    to[1][2] = from[1][2];
    to[1][3] = from[1][3];

    to[2][0] = from[2][0];
    to[2][1] = from[2][1];
    to[2][2] = from[2][2];
    to[2][3] = from[2][3];

    to[3][0] = from[3][0];
    to[3][1] = from[3][1];
    to[3][2] = from[3][2];
    to[3][3] = from[3][3];
}

void
mat4_mul(te_mat4 m1, te_mat4 m2, te_mat4 dst) {
    float a00 = m1[0][0], a01 = m1[0][1], a02 = m1[0][2], a03 = m1[0][3], a10 = m1[1][0],
          a11 = m1[1][1], a12 = m1[1][2], a13 = m1[1][3], a20 = m1[2][0], a21 = m1[2][1],
          a22 = m1[2][2], a23 = m1[2][3], a30 = m1[3][0], a31 = m1[3][1], a32 = m1[3][2],
          a33 = m1[3][3],

          b00 = m2[0][0], b01 = m2[0][1], b02 = m2[0][2], b03 = m2[0][3], b10 = m2[1][0],
          b11 = m2[1][1], b12 = m2[1][2], b13 = m2[1][3], b20 = m2[2][0], b21 = m2[2][1],
          b22 = m2[2][2], b23 = m2[2][3], b30 = m2[3][0], b31 = m2[3][1], b32 = m2[3][2],
          b33 = m2[3][3];

    dst[0][0] = a00 * b00 + a10 * b01 + a20 * b02 + a30 * b03;
    dst[0][1] = a01 * b00 + a11 * b01 + a21 * b02 + a31 * b03;
    dst[0][2] = a02 * b00 + a12 * b01 + a22 * b02 + a32 * b03;
    dst[0][3] = a03 * b00 + a13 * b01 + a23 * b02 + a33 * b03;
    dst[1][0] = a00 * b10 + a10 * b11 + a20 * b12 + a30 * b13;
    dst[1][1] = a01 * b10 + a11 * b11 + a21 * b12 + a31 * b13;
    dst[1][2] = a02 * b10 + a12 * b11 + a22 * b12 + a32 * b13;
    dst[1][3] = a03 * b10 + a13 * b11 + a23 * b12 + a33 * b13;
    dst[2][0] = a00 * b20 + a10 * b21 + a20 * b22 + a30 * b23;
    dst[2][1] = a01 * b20 + a11 * b21 + a21 * b22 + a31 * b23;
    dst[2][2] = a02 * b20 + a12 * b21 + a22 * b22 + a32 * b23;
    dst[2][3] = a03 * b20 + a13 * b21 + a23 * b22 + a33 * b23;
    dst[3][0] = a00 * b30 + a10 * b31 + a20 * b32 + a30 * b33;
    dst[3][1] = a01 * b30 + a11 * b31 + a21 * b32 + a31 * b33;
    dst[3][2] = a02 * b30 + a12 * b31 + a22 * b32 + a32 * b33;
    dst[3][3] = a03 * b30 + a13 * b31 + a23 * b32 + a33 * b33;
}

void
mat4_mulv(te_mat4 mat, te_vec4 v, te_vec4 dst) {
    te_vec4 out; /* temporary needed in case dst is input */
    out[0] = mat[0][0] * v[0] + mat[1][0] * v[1] + mat[2][0] * v[2] + mat[3][0] * v[3];
    out[1] = mat[0][1] * v[0] + mat[1][1] * v[1] + mat[2][1] * v[2] + mat[3][1] * v[3];
    out[2] = mat[0][2] * v[0] + mat[1][2] * v[1] + mat[2][2] * v[2] + mat[3][2] * v[3];
    out[3] = mat[0][3] * v[0] + mat[1][3] * v[1] + mat[2][3] * v[2] + mat[3][3] * v[3];
    vec4_copy(out, dst);
}

void
mat4_inv(te_mat4 mat, te_mat4 dst) {
    float a = mat[0][0], b = mat[0][1], c = mat[0][2], d = mat[0][3], e = mat[1][0],
          f = mat[1][1], g = mat[1][2], h = mat[1][3], i = mat[2][0], j = mat[2][1],
          k = mat[2][2], l = mat[2][3], m = mat[3][0], n = mat[3][1], o = mat[3][2],
          p = mat[3][3],

          c1 = k * p - l * o, c2 = c * h - d * g, c3 = i * p - l * m, c4 = a * h - d * e,
          c5 = j * p - l * n, c6 = b * h - d * f, c7 = i * n - j * m, c8 = a * f - b * e,
          c9 = j * o - k * n, c10 = b * g - c * f, c11 = i * o - k * m, c12 = a * g - c * e,

          idt = 1.0f / (c8 * c1 + c4 * c9 + c10 * c3 + c2 * c7 - c12 * c5 - c6 * c11),
          ndt = -idt;

    dst[0][0] = (f * c1 - g * c5 + h * c9) * idt;
    dst[0][1] = (b * c1 - c * c5 + d * c9) * ndt;
    dst[0][2] = (n * c2 - o * c6 + p * c10) * idt;
    dst[0][3] = (j * c2 - k * c6 + l * c10) * ndt;

    dst[1][0] = (e * c1 - g * c3 + h * c11) * ndt;
    dst[1][1] = (a * c1 - c * c3 + d * c11) * idt;
    dst[1][2] = (m * c2 - o * c4 + p * c12) * ndt;
    dst[1][3] = (i * c2 - k * c4 + l * c12) * idt;

    dst[2][0] = (e * c5 - f * c3 + h * c7) * idt;
    dst[2][1] = (a * c5 - b * c3 + d * c7) * ndt;
    dst[2][2] = (m * c6 - n * c4 + p * c8) * idt;
    dst[2][3] = (i * c6 - j * c4 + l * c8) * ndt;

    dst[3][0] = (e * c9 - f * c11 + g * c7) * ndt;
    dst[3][1] = (a * c9 - b * c11 + c * c7) * idt;
    dst[3][2] = (m * c10 - n * c12 + o * c8) * ndt;
    dst[3][3] = (i * c10 - j * c12 + k * c8) * idt;
}

void
mat4_transpose(te_mat4 mat, te_mat4 dst) {
    te_mat4 out; /* temporary needed in case dst is input */
    out[0][0] = mat[0][0];
    out[1][0] = mat[0][1];
    out[0][1] = mat[1][0];
    out[1][1] = mat[1][1];
    out[0][2] = mat[2][0];
    out[1][2] = mat[2][1];
    out[0][3] = mat[3][0];
    out[1][3] = mat[3][1];
    out[2][0] = mat[0][2];
    out[3][0] = mat[0][3];
    out[2][1] = mat[1][2];
    out[3][1] = mat[1][3];
    out[2][2] = mat[2][2];
    out[3][2] = mat[2][3];
    out[2][3] = mat[3][2];
    out[3][3] = mat[3][3];

    mat4_copy(out, dst);
}

void
mat4_set_translation_part(te_mat4 mat, te_vec3 translation) {
    mat[3][0] = translation[0];
    mat[3][1] = translation[1];
    mat[3][2] = translation[2];
}

void
mat4_make_rotate_from_norm_axis(te_mat4 mat, float angle, te_vec3 normalized_axis) {
    te_vec3 axis_cos;
    te_vec3 axis_sin;
    float cos_angle = math_cos(angle);

#if defined(DEBUG)
    {
        /* make sure we are given a normalized axis */
        float len = vec3_len(normalized_axis);
        if (len >= 1.01f || len <= 0.99f) {
            log_error(
                __FILE__, __LINE__, "the specified axis vector should have been normalized");
            abort();
        }
    }
#endif

    vec3_muls(normalized_axis, 1.0f - cos_angle, axis_cos);
    vec3_muls(normalized_axis, math_sin(angle), axis_sin);

    vec3_muls(normalized_axis, axis_cos[0], mat[0]);
    vec3_muls(normalized_axis, axis_cos[1], mat[1]);
    vec3_muls(normalized_axis, axis_cos[2], mat[2]);

    mat[0][0] += cos_angle;
    mat[1][0] -= axis_sin[2];
    mat[2][0] += axis_sin[1];
    mat[0][1] += axis_sin[2];
    mat[1][1] += cos_angle;
    mat[2][1] -= axis_sin[0];
    mat[0][2] -= axis_sin[1];
    mat[1][2] += axis_sin[0];
    mat[2][2] += cos_angle;

    mat[0][3] = 0.0f;
    mat[1][3] = 0.0f;
    mat[2][3] = 0.0f;

    mat[3][0] = 0.0f;
    mat[3][1] = 0.0f;
    mat[3][2] = 0.0f;
    mat[3][3] = 1.0f;
}

void
mat4_set_scaling_part(te_mat4 mat, te_vec3 scale) {
    mat[0][0] = scale[0];
    mat[1][1] = scale[1];
    mat[2][2] = scale[2];
}

void
mat4_make_view_mat_rh(
    te_vec3 camera_pos, te_vec3 norm_camera_forward, te_vec3 norm_camera_right,
    te_vec3 norm_camera_up, te_mat4 dst) {
#if defined(DEBUG)
    {
        /* make sure we are given a normalized vector */
        float len = vec3_len(norm_camera_forward);
        if (len >= 1.01f || len <= 0.99f) {
            log_error(
                __FILE__, __LINE__, "the specified axis vector should have been normalized");
            abort();
        }
        len = vec3_len(norm_camera_right);
        if (len >= 1.01f || len <= 0.99f) {
            log_error(
                __FILE__, __LINE__, "the specified axis vector should have been normalized");
            abort();
        }
        len = vec3_len(norm_camera_up);
        if (len >= 1.01f || len <= 0.99f) {
            log_error(
                __FILE__, __LINE__, "the specified axis vector should have been normalized");
            abort();
        }
    }
#endif

    dst[0][0] = norm_camera_right[0];
    dst[0][1] = norm_camera_up[0];
    dst[0][2] = -norm_camera_forward[0];
    dst[0][3] = 0.0f;

    dst[1][0] = norm_camera_right[1];
    dst[1][1] = norm_camera_up[1];
    dst[1][2] = -norm_camera_forward[1];
    dst[1][3] = 0.0f;

    dst[2][0] = norm_camera_right[2];
    dst[2][1] = norm_camera_up[2];
    dst[2][2] = -norm_camera_forward[2];
    dst[2][3] = 0.0f;

    dst[3][0] = -vec3_dot(norm_camera_right, camera_pos);
    dst[3][1] = -vec3_dot(norm_camera_up, camera_pos);
    dst[3][2] = vec3_dot(norm_camera_forward, camera_pos);
    dst[3][3] = 1.0f;
}

void
mat4_make_proj_mat_rh(
    float fov_y_rad, float aspect_ratio, float near_z, float far_z, te_mat4 dst) {
    float fov_tan = 1.0f / math_tan(fov_y_rad * 0.5f);
    float inv_z = 1.0f / (near_z - far_z);

    dst[0][0] = fov_tan / aspect_ratio;
    dst[0][1] = 0.0f;
    dst[0][2] = 0.0f;
    dst[0][3] = 0.0f;

    dst[1][0] = 0.0f;
    dst[1][1] = fov_tan;
    dst[1][2] = 0.0f;
    dst[1][3] = 0.0f;

    dst[2][0] = 0.0f;
    dst[2][1] = 0.0f;
    dst[2][2] = (near_z + far_z) * inv_z;
    dst[2][3] = -1.0f;

    dst[3][0] = 0.0f;
    dst[3][1] = 0.0f;
    dst[3][2] = 2.0f * near_z * far_z * inv_z;
    dst[3][3] = 0.0f;
}

void
mat4_extract_euler_angles_rad(te_mat4 mat, te_vec3 dst) {
    float m00, m01, m10, m11, m20, m21, m22;
    float theta_x, theta_y, theta_z;

    m00 = mat[0][0];
    m10 = mat[1][0];
    m20 = mat[2][0];
    m01 = mat[0][1];
    m11 = mat[1][1];
    m21 = mat[2][1];
    m22 = mat[2][2];

    if (m20 < 1.0f) {
        if (m20 > -1.0f) {
            theta_y = math_asin(m20);
            theta_x = math_atan2(-m21, m22);
            theta_z = math_atan2(-m10, m00);
        } else {
            theta_y = -1.57079632f;
            theta_x = -math_atan2(m01, m11);
            theta_z = 0.0f;
        }
    } else {
        theta_y = 1.57079632f;
        theta_x = math_atan2(m01, m11);
        theta_z = 0.0f;
    }

    dst[0] = theta_x;
    dst[1] = theta_y;
    dst[2] = theta_z;
}

void
mat4_to_quat(te_mat4 mat, te_vec4 dst) {
    float trace, s, sinv;

    trace = mat[0][0] + mat[1][1] + mat[2][2];
    if (trace >= 0.0f) {
        s = math_sqrt(1.0f + trace);
        sinv = 0.5f / s;

        dst[0] = sinv * (mat[1][2] - mat[2][1]);
        dst[1] = sinv * (mat[2][0] - mat[0][2]);
        dst[2] = sinv * (mat[0][1] - mat[1][0]);
        dst[3] = s * 0.5f;
    } else if (mat[0][0] >= mat[1][1] && mat[0][0] >= mat[2][2]) {
        s = math_sqrt(1.0f - mat[1][1] - mat[2][2] + mat[0][0]);
        sinv = 0.5f / s;

        dst[0] = s * 0.5f;
        dst[1] = sinv * (mat[0][1] + mat[1][0]);
        dst[2] = sinv * (mat[0][2] + mat[2][0]);
        dst[3] = sinv * (mat[1][2] - mat[2][1]);
    } else if (mat[1][1] >= mat[2][2]) {
        s = math_sqrt(1.0f - mat[0][0] - mat[2][2] + mat[1][1]);
        sinv = 0.5f / s;

        dst[0] = sinv * (mat[0][1] + mat[1][0]);
        dst[1] = s * 0.5f;
        dst[2] = sinv * (mat[1][2] + mat[2][1]);
        dst[3] = sinv * (mat[2][0] - mat[0][2]);
    } else {
        s = math_sqrt(1.0f - mat[0][0] - mat[1][1] + mat[2][2]);
        sinv = 0.5f / s;

        dst[0] = sinv * (mat[0][2] + mat[2][0]);
        dst[1] = sinv * (mat[1][2] + mat[2][1]);
        dst[2] = s * 0.5f;
        dst[3] = sinv * (mat[0][1] - mat[1][0]);
    }
}

void
mat4_from_quat(te_vec4 quat, te_mat4 dst) {
    float w, x, y, z, xx, yy, zz, xy, yz, xz, wx, wy, wz, norm, s;

    norm = vec4_len(quat);
    s = norm > 0.0f ? 2.0f / norm : 0.0f;

    x = quat[0];
    y = quat[1];
    z = quat[2];
    w = quat[3];

    xx = s * x * x;
    xy = s * x * y;
    wx = s * w * x;
    yy = s * y * y;
    yz = s * y * z;
    wy = s * w * y;
    zz = s * z * z;
    xz = s * x * z;
    wz = s * w * z;

    dst[0][0] = 1.0f - yy - zz;
    dst[1][1] = 1.0f - xx - zz;
    dst[2][2] = 1.0f - xx - yy;

    dst[0][1] = xy + wz;
    dst[1][2] = yz + wx;
    dst[2][0] = xz + wy;

    dst[1][0] = xy - wz;
    dst[2][1] = yz - wx;
    dst[0][2] = xz - wy;

    dst[0][3] = 0.0f;
    dst[1][3] = 0.0f;
    dst[2][3] = 0.0f;
    dst[3][0] = 0.0f;
    dst[3][1] = 0.0f;
    dst[3][2] = 0.0f;
    dst[3][3] = 1.0f;
}

void
mat4_decompose(te_mat4 mat, te_vec4 out_translation, te_mat4 out_rotation, te_vec3 out_scale) {
    te_vec3 cross;

    vec4_copy(mat[3], out_translation);

    vec4_copy(mat[0], out_rotation[0]);
    vec4_copy(mat[1], out_rotation[1]);
    vec4_copy(mat[2], out_rotation[2]);
    vec4_set(0.0f, 0.0f, 0.0f, 1.0f, out_rotation[3]);

    out_scale[0] = vec3_len(mat[0]);
    out_scale[1] = vec3_len(mat[1]);
    out_scale[2] = vec3_len(mat[2]);

    vec4_muls(out_rotation[0], 1.0f / out_scale[0], out_rotation[0]);
    vec4_muls(out_rotation[1], 1.0f / out_scale[1], out_rotation[1]);
    vec4_muls(out_rotation[2], 1.0f / out_scale[2], out_rotation[2]);

    vec3_cross(mat[0], mat[1], cross);
    if (vec3_dot(cross, mat[2]) < 0.0f) {
        vec4_muls(out_rotation[0], -1.0f, out_rotation[0]);
        vec4_muls(out_rotation[1], -1.0f, out_rotation[1]);
        vec4_muls(out_rotation[2], -1.0f, out_rotation[2]);
        vec3_muls(out_scale, -1.0f, out_scale);
    }
}
