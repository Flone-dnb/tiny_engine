#pragma once

typedef float te_vec2[2];

void vec2_set(float x, float y, te_vec2 out);
void vec2_zero(te_vec2 v);

void vec2_copy(te_vec2 src, te_vec2 dst);

/* component-wise operation A to B */
void vec2_add(te_vec2 a, te_vec2 b, te_vec2 dst);
void vec2_sub(te_vec2 a, te_vec2 b, te_vec2 dst);
void vec2_div(te_vec2 a, te_vec2 b, te_vec2 dst);
void vec2_mul(te_vec2 a, te_vec2 b, te_vec2 dst);

/* component-wise operation with a scalar */
void vec2_adds(te_vec2 a, float b, te_vec2 dst);
void vec2_subs(te_vec2 a, float b, te_vec2 dst);
void vec2_muls(te_vec2 a, float b, te_vec2 dst);
void vec2_divs(te_vec2 a, float b, te_vec2 dst);

/* linear interpolation [from + t * (to - from)] based on [0.0; 1.0] factor */
void vec2_lerp(te_vec2 from, te_vec2 to, float t, te_vec2 dst);

/* dot product */
float vec2_dot(te_vec2 a, te_vec2 b);

/* magnitude (length) of the vector */
float vec2_len(te_vec2 v);

/* normalizes the vector, also checks if length is zero to avoid NaNs */
void vec2_normalize(te_vec2 v);
