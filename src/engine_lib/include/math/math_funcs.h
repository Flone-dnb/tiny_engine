#pragma once

#include <cglm/mat4.h>
#include <math/vec2.h>
#include <math/vec3.h>

float math_abs(float value);

/* used to fix a common problem where diagonal movement has ~1.4 speed instead of 1 */
void math_fix_diagonal_movement_speedup(te_vec2 movement);

/* converts a normalized direction vector to rotation angles */
void math_convert_norm_dir_to_rot(te_vec3 dir, te_vec3 out);

/* creates a new rotation matrix from a rotation (in degrees) */
void math_make_rotation_mat(te_vec3 rotation_deg, mat4 out);

/* converts rotation angles to a normalized direction vector */
void math_convert_rot_to_norm_dir(te_vec3 rot, te_vec3 out);
