#include <math/vec3.h>

#include <math.h>

void
vec3_set(float x, float y, float z, te_vec3 out) {
    out[0] = x;
    out[1] = y;
    out[2] = z;
}

void
vec3_zero(te_vec3 v) {
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

void
vec3_copy(te_vec3 src, te_vec3 dst) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

void
vec3_add(te_vec3 a, te_vec3 b, te_vec3 dst) {
    dst[0] = a[0] + b[0];
    dst[1] = a[1] + b[1];
    dst[2] = a[2] + b[2];
}

void
vec3_sub(te_vec3 a, te_vec3 b, te_vec3 dst) {
    dst[0] = a[0] - b[0];
    dst[1] = a[1] - b[1];
    dst[2] = a[2] - b[2];
}

void
vec3_div(te_vec3 a, te_vec3 b, te_vec3 dst) {
    dst[0] = a[0] / b[0];
    dst[1] = a[1] / b[1];
    dst[2] = a[2] / b[2];
}

void
vec3_mul(te_vec3 a, te_vec3 b, te_vec3 dst) {
    dst[0] = a[0] * b[0];
    dst[1] = a[1] * b[1];
    dst[2] = a[2] * b[2];
}

void
vec3_muls(te_vec3 a, float b, te_vec3 dst) {
    dst[0] = a[0] * b;
    dst[1] = a[1] * b;
    dst[2] = a[2] * b;
}

void
vec3_adds(te_vec3 a, float b, te_vec3 dst) {
    dst[0] = a[0] + b;
    dst[1] = a[1] + b;
    dst[2] = a[2] + b;
}

void
vec3_lerp(te_vec3 from, te_vec3 to, float t, te_vec3 dst) {
    te_vec3 tmp;

    vec3_sub(to, from, tmp);
    vec3_muls(tmp, t, tmp);
    vec3_add(from, tmp, dst);
}

float
vec3_dot(te_vec3 a, te_vec3 b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float
vec3_len(te_vec3 v) {
    return (float)sqrt(vec3_dot(v, v));
}

void
vec3_cross(te_vec3 a, te_vec3 b, te_vec3 dst) {
    dst[0] = a[1] * b[2] - a[2] * b[1];
    dst[1] = a[2] * b[0] - a[0] * b[2];
    dst[2] = a[0] * b[1] - a[1] * b[0];
}

void
vec3_normalize(te_vec3 v) {
    float tmp;

    tmp = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (tmp < 0.00001f) {
        v[0] = 0.0f;
        v[1] = 0.0f;
        v[2] = 0.0f;
        return;
    }

    tmp = 1.0f / (float)sqrt(tmp);
    v[0] *= tmp;
    v[1] *= tmp;
    v[2] *= tmp;
}
