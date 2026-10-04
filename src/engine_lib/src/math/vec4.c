#include <math/vec4.h>

#include <math.h>

void
vec4_set(float x, float y, float z, float w, te_vec4 out) {
    out[0] = x;
    out[1] = y;
    out[2] = z;
    out[3] = w;
}

void
vec4_zero(te_vec4 v) {
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
    v[3] = 0.0f;
}

void
vec4_copy(te_vec4 src, te_vec4 dst) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

void
vec4_add(te_vec4 a, te_vec4 b, te_vec4 dst) {
    dst[0] = a[0] + b[0];
    dst[1] = a[1] + b[1];
    dst[2] = a[2] + b[2];
    dst[3] = a[3] + b[3];
}

void
vec4_sub(te_vec4 a, te_vec4 b, te_vec4 dst) {
    dst[0] = a[0] - b[0];
    dst[1] = a[1] - b[1];
    dst[2] = a[2] - b[2];
    dst[3] = a[3] - b[3];
}

void
vec4_div(te_vec4 a, te_vec4 b, te_vec4 dst) {
    dst[0] = a[0] / b[0];
    dst[1] = a[1] / b[1];
    dst[2] = a[2] / b[2];
    dst[3] = a[3] / b[3];
}

void
vec4_mul(te_vec4 a, te_vec4 b, te_vec4 dst) {
    dst[0] = a[0] * b[0];
    dst[1] = a[1] * b[1];
    dst[2] = a[2] * b[2];
    dst[3] = a[3] * b[3];
}

void
vec4_adds(te_vec4 a, float b, te_vec4 dst) {
    dst[0] = a[0] + b;
    dst[1] = a[1] + b;
    dst[2] = a[2] + b;
    dst[3] = a[3] + b;
}

void
vec4_subs(te_vec4 a, float b, te_vec4 dst) {
    dst[0] = a[0] - b;
    dst[1] = a[1] - b;
    dst[2] = a[2] - b;
    dst[3] = a[3] - b;
}

void
vec4_muls(te_vec4 a, float b, te_vec4 dst) {
    dst[0] = a[0] * b;
    dst[1] = a[1] * b;
    dst[2] = a[2] * b;
    dst[3] = a[3] * b;
}

void
vec4_divs(te_vec4 a, float b, te_vec4 dst) {
    dst[0] = a[0] / b;
    dst[1] = a[1] / b;
    dst[2] = a[2] / b;
    dst[3] = a[3] / b;
}

void
vec4_lerp(te_vec4 from, te_vec4 to, float t, te_vec4 dst) {
    te_vec4 tmp;

    vec4_sub(to, from, tmp);
    vec4_muls(tmp, t, tmp);
    vec4_add(from, tmp, dst);
}

float
vec4_dot(te_vec4 a, te_vec4 b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

float
vec4_len(te_vec4 v) {
    return (float)sqrt(vec4_dot(v, v));
}

void
vec4_normalize(te_vec4 v) {
    float tmp;

    tmp = v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3];
    if (tmp < 0.00001f) {
        v[0] = 0.0f;
        v[1] = 0.0f;
        v[2] = 0.0f;
        v[3] = 0.0f;
        return;
    }

    tmp = 1.0f / (float)sqrt(tmp);
    v[0] *= tmp;
    v[1] *= tmp;
    v[2] *= tmp;
    v[3] *= tmp;
}

void
vec4_slerp(te_vec4 from, te_vec4 to, float t, te_vec4 dst) {
    te_vec4 q1, q2;
    float cos_theta, sin_theta, angle;

    cos_theta = vec4_dot(from, to);
    vec4_copy(from, q1);

    if (fabs(cos_theta) >= 1.0) {
        vec4_copy(q1, dst);
        return;
    }

    if (cos_theta < 0.0f) {
        vec4_muls(q1, -1.0f, q1);
        cos_theta = -cos_theta;
    }

    sin_theta = (float)sqrt(1.0f - cos_theta * cos_theta);

    if (fabs(sin_theta) < 0.001) {
        vec4_lerp(from, to, t, dst);
        return;
    }

    angle = (float)acos(cos_theta);
    vec4_muls(q1, (float)sin((1.0f - t) * angle), q1);
    vec4_muls(to, (float)sin(t * angle), q2);

    vec4_add(q1, q2, q1);
    vec4_muls(q1, 1.0f / sin_theta, dst);
}
