#pragma once

typedef float te_vec4[4];

void vec4_set(float x, float y, float z, float w, te_vec4 out);
void vec4_zero(te_vec4 v);

void vec4_copy(te_vec4 src, te_vec4 dst);

/* component-wise operation A to B */
void vec4_add(te_vec4 a, te_vec4 b, te_vec4 dst);
void vec4_sub(te_vec4 a, te_vec4 b, te_vec4 dst);
void vec4_div(te_vec4 a, te_vec4 b, te_vec4 dst);
void vec4_mul(te_vec4 a, te_vec4 b, te_vec4 dst);

/* component-wise operation with a scalar */
void vec4_adds(te_vec4 a, float b, te_vec4 dst);
void vec4_subs(te_vec4 a, float b, te_vec4 dst);
void vec4_muls(te_vec4 a, float b, te_vec4 dst);
void vec4_divs(te_vec4 a, float b, te_vec4 dst);

/* linear interpolation [from + t * (to - from)] based on [0.0; 1.0] factor */
void vec4_lerp(te_vec4 from, te_vec4 to, float t, te_vec4 dst);

/* dot product */
float vec4_dot(te_vec4 a, te_vec4 b);

/* magnitude (length) of the vector */
float vec4_len(te_vec4 v);

/* normalizes the vector, also checks if length is zero to avoid NaNs */
void vec4_normalize(te_vec4 v);

/** spherical linear interpolation */
void vec4_slerp(te_vec4 from, te_vec4 to, float t, te_vec4 dst);
