#pragma once

typedef float te_vec3[3];

void vec3_set(float x, float y, float z, te_vec3 out);
void vec3_zero(te_vec3 v);

void vec3_copy(te_vec3 src, te_vec3 dst);

/* component-wise operation A to B */
void vec3_add(te_vec3 a, te_vec3 b, te_vec3 dst);
void vec3_sub(te_vec3 a, te_vec3 b, te_vec3 dst);
void vec3_div(te_vec3 a, te_vec3 b, te_vec3 dst);
void vec3_mul(te_vec3 a, te_vec3 b, te_vec3 dst);

/* component-wise multiplication/addition with a scalar */
void vec3_muls(te_vec3 a, float b, te_vec3 dst);
void vec3_adds(te_vec3 a, float b, te_vec3 dst);

/* linear interpolation [from + t * (to - from)] based on [0.0; 1.0] factor */
void vec3_lerp(te_vec3 from, te_vec3 to, float t, te_vec3 dst);

/* dot and cross product */
float vec3_dot(te_vec3 a, te_vec3 b);
void vec3_cross(te_vec3 a, te_vec3 b, te_vec3 dst);

/* magnitude (length) of the vector */
float vec3_len(te_vec3 v);

/* normalizes the vector, also checks if length is zero to avoid NaNs */
void vec3_normalize(te_vec3 v);
