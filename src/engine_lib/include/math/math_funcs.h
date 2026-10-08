#pragma once

#include <math/mat4.h>
#include <math/vec2.h>

float math_pi(void);
float math_abs(float value);
float math_max(float a, float b);
float math_min(float a, float b);
float math_clamp(float value, float min, float max);
float math_rad(float deg);
float math_deg(float rad);
float math_tan(float value);
float math_round(float value);

/* returns value between 0 (if x < edge0) and 1 (if x > edge1) */
float math_smoothstep(float edge0, float edge1, float x);

/* linear interpolation [from + t * (to - from)] based on [0.0; 1.0] factor */
float math_lerp(float from, float to, float t);

/* used to fix a common problem where diagonal movement has ~1.4 speed instead of 1 */
void math_fix_diagonal_movement_speedup(te_vec2 movement);

/* converts a normalized direction vector to rotation angles */
void math_convert_norm_dir_to_rot(te_vec3 dir, te_vec3 out);

/* creates a new rotation matrix from a rotation (in degrees) */
void math_make_rotation_mat(te_vec3 rotation_deg, te_mat4 out);

/* converts rotation angles to a normalized direction vector */
void math_convert_rot_to_norm_dir(te_vec3 rot, te_vec3 out);
