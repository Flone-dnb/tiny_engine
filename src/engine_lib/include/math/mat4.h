#pragma once

#include <math/vec3.h>
#include <math/vec4.h>

typedef te_vec4 te_mat4[4];

void mat4_identity(te_mat4 mat);
void mat4_copy(te_mat4 from, te_mat4 to);

void mat4_mul(te_mat4 m1, te_mat4 m2, te_mat4 dst);
void mat4_mulv(te_mat4 mat, te_vec4 v, te_vec4 dst);

void mat4_inv(te_mat4 mat, te_mat4 dst);
void mat4_transpose(te_mat4 mat, te_mat4 dst);

/* overwrites the translation component of the matrix */
void mat4_set_translation_part(te_mat4 mat, te_vec3 translation);

/* overwrites the scaling component of the matrix */
void mat4_set_scaling_part(te_mat4 mat, te_vec3 scale);

/* overwrites all components and makes a rotation matrix */
void mat4_make_rotate_from_norm_axis(te_mat4 mat, float angle, te_vec3 normalized_axis);

void mat4_make_view_mat_rh(
    te_vec3 camera_pos, te_vec3 norm_camera_forward, te_vec3 norm_camera_right,
    te_vec3 norm_camera_up, te_mat4 dst);

void mat4_make_proj_mat_rh(
    float fov_y_rad, float aspect_ratio, float near_z, float far_z, te_mat4 dst);

void mat4_extract_euler_angles_rad(te_mat4 mat, te_vec3 dst);

void mat4_to_quat(te_mat4 mat, te_vec4 dst);
void mat4_from_quat(te_vec4 quat, te_mat4 dst);

void
mat4_decompose(te_mat4 mat, te_vec4 out_translation, te_mat4 out_rotation, te_vec3 out_scale);
