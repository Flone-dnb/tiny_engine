#include <math/vec2.h>

#include <math.h>

void
vec2_set(float x, float y, te_vec2 out) {
    out[0] = x;
    out[1] = y;
}

void
vec2_zero(te_vec2 v) {
    v[0] = 0.0f;
    v[1] = 0.0f;
}

void
vec2_copy(te_vec2 src, te_vec2 dst) {
    dst[0] = src[0];
    dst[1] = src[1];
}

void
vec2_add(te_vec2 a, te_vec2 b, te_vec2 dst) {
    dst[0] = a[0] + b[0];
    dst[1] = a[1] + b[1];
}

void
vec2_sub(te_vec2 a, te_vec2 b, te_vec2 dst) {
    dst[0] = a[0] - b[0];
    dst[1] = a[1] - b[1];
}

void
vec2_div(te_vec2 a, te_vec2 b, te_vec2 dst) {
    dst[0] = a[0] / b[0];
    dst[1] = a[1] / b[1];
}

void
vec2_mul(te_vec2 a, te_vec2 b, te_vec2 dst) {
    dst[0] = a[0] * b[0];
    dst[1] = a[1] * b[1];
}

void
vec2_muls(te_vec2 a, float b, te_vec2 dst) {
    dst[0] = a[0] * b;
    dst[1] = a[1] * b;
}

void
vec2_adds(te_vec2 a, float b, te_vec2 dst) {
    dst[0] = a[0] + b;
    dst[1] = a[1] + b;
}

void
vec2_lerp(te_vec2 from, te_vec2 to, float t, te_vec2 dst) {
    te_vec2 tmp;

    vec2_sub(to, from, tmp);
    vec2_muls(tmp, t, tmp);
    vec2_add(from, tmp, dst);
}

float
vec2_dot(te_vec2 a, te_vec2 b) {
    return a[0] * b[0] + a[1] * b[1];
}

float
vec2_len(te_vec2 v) {
    return (float)sqrt(vec2_dot(v, v));
}

void
vec2_normalize(te_vec2 v) {
    float tmp;

    tmp = v[0] * v[0] + v[1] * v[1];
    if (tmp < 0.00001f) {
        v[0] = 0.0f;
        v[1] = 0.0f;
        return;
    }

    tmp = 1.0f / (float)sqrt(tmp);
    v[0] *= tmp;
    v[1] *= tmp;
}
