#include <math/mat3.h>

void
mat3_identity(te_mat3 mat) {
    mat[0][0] = 1.0f;
    mat[0][1] = 0.0f;
    mat[0][2] = 0.0f;

    mat[1][0] = 0.0f;
    mat[1][1] = 1.0f;
    mat[1][2] = 0.0f;

    mat[2][0] = 0.0f;
    mat[2][1] = 0.0f;
    mat[2][2] = 1.0f;
}

void
mat3_copy(te_mat3 from, te_mat3 to) {
    to[0][0] = from[0][0];
    to[0][1] = from[0][1];
    to[0][2] = from[0][2];

    to[1][0] = from[1][0];
    to[1][1] = from[1][1];
    to[1][2] = from[1][2];

    to[2][0] = from[2][0];
    to[2][1] = from[2][1];
    to[2][2] = from[2][2];
}

void
mat3_from_mat4(te_mat4 from, te_mat3 to) {
    to[0][0] = from[0][0];
    to[0][1] = from[0][1];
    to[0][2] = from[0][2];

    to[1][0] = from[1][0];
    to[1][1] = from[1][1];
    to[1][2] = from[1][2];

    to[2][0] = from[2][0];
    to[2][1] = from[2][1];
    to[2][2] = from[2][2];
}
