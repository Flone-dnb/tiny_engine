#pragma once

#include <cglm/mat4.h>
#include <cglm/vec2.h>
#include <cglm/vec3.h>

/* used to fix a common problem where diagonal movement has ~1.4 speed instead of 1 */
void math_fix_diagonal_movement_speedup(vec2 movement);

/* normalizes the specified vector while checking for zero division (to avoid NaNs in the normalized vector) */
void math_normalize_safely(vec3 vec);

/* converts a normalized direction vector to rotation angles */
void math_convert_norm_dir_to_rot(vec3 dir, vec3 out);

/* creates a new rotation matrix from a rotation (in degrees) */
void math_make_rotation_mat(vec3 rotation_deg, mat4 out);

/* converts rotation angles to a normalized direction vector */
void math_convert_rot_to_norm_dir(vec3 rot, vec3 out);
