#pragma once

#include <math/vec3.h>
#include <math/mat4.h>

typedef te_vec3 te_mat3[3];

void mat3_identity(te_mat3 mat);
void mat3_copy(te_mat3 from, te_mat3 to);
void mat3_from_mat4(te_mat4 from, te_mat3 to);
