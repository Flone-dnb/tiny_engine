#include "misc/mesh_generator.h"

#include <stdlib.h>
#include <string.h>
#include <game/model.h>

void
mesh_generator_plane(
    te_vertex_pack** vertices, unsigned short** indices, unsigned int* index_count) {
    const float half = 0.5f;
    unsigned int vert_size;
    unsigned int i;
    unsigned char offset;

    (*vertices) = vertex_pack_create(4, false);

    vert_size = (*vertices)->vertex_sizeof;
    offset = (*vertices)->attribute_offsets[TE_VA_UV];

    vec2_set(0.0f, 0.0f, (float*)((*vertices)->data + (vert_size * 0 + offset)));
    vec2_set(1.0f, 0.0f, (float*)((*vertices)->data + (vert_size * 1 + offset)));
    vec2_set(0.0f, 1.0f, (float*)((*vertices)->data + (vert_size * 2 + offset)));
    vec2_set(1.0f, 1.0f, (float*)((*vertices)->data + (vert_size * 3 + offset)));

    offset = (*vertices)->attribute_offsets[TE_VA_NORMAL];
    for (i = 0; i < 4; i++) {
        vec3_set(0.0f, 0.0f, 1.0f, (float*)((*vertices)->data + (vert_size * i + offset)));
    }

    offset = (*vertices)->attribute_offsets[TE_VA_POSITION];

    vec3_set(-half, half, -half, (float*)((*vertices)->data + (vert_size * 0 + offset)));
    vec3_set(half, half, -half, (float*)((*vertices)->data + (vert_size * 1 + offset)));
    vec3_set(-half, -half, -half, (float*)((*vertices)->data + (vert_size * 2 + offset)));
    vec3_set(half, -half, -half, (float*)((*vertices)->data + (vert_size * 3 + offset)));

    (*index_count) = 6;
    (*indices) = malloc(sizeof(unsigned short) * (*index_count));
    (*indices)[0] = 0;
    (*indices)[1] = 2;
    (*indices)[2] = 1;
    (*indices)[3] = 3;
    (*indices)[4] = 1;
    (*indices)[5] = 2;
}

void
mesh_generator_cube(
    struct te_vertex_pack** vertices, unsigned short** indices, unsigned int* index_count) {
    const float half = 0.5f;
    unsigned int vert_size;
    unsigned int i;
    unsigned int normal_i;
    unsigned char offset;

    (*vertices) = vertex_pack_create(24, false);

    vert_size = (*vertices)->vertex_sizeof;

    /* init UVs */
    offset = (*vertices)->attribute_offsets[TE_VA_UV];
    for (i = 0; i < (*vertices)->vertex_count; i += 4) {
        vec2_set(1.0f, 1.0f, (float*)((*vertices)->data + (vert_size * i + offset)));
        vec2_set(0.0f, 1.0f, (float*)((*vertices)->data + (vert_size * (i + 1) + offset)));
        vec2_set(1.0f, 0.0f, (float*)((*vertices)->data + (vert_size * (i + 2) + offset)));
        vec2_set(0.0f, 0.0f, (float*)((*vertices)->data + (vert_size * (i + 3) + offset)));
    }

    /* init normals */
    offset = (*vertices)->attribute_offsets[TE_VA_NORMAL];
    normal_i = 0;
    for (i = normal_i; normal_i < i + 4; normal_i++) {
        vec3_set(
            1.0f, 0.0f, 0.0f, (float*)((*vertices)->data + (vert_size * normal_i + offset)));
    }
    for (i = normal_i; normal_i < i + 4; normal_i++) {
        vec3_set(
            -1.0f, 0.0f, 0.0f, (float*)((*vertices)->data + (vert_size * normal_i + offset)));
    }
    for (i = normal_i; normal_i < i + 4; normal_i++) {
        vec3_set(
            0.0f, 1.0f, 0.0f, (float*)((*vertices)->data + (vert_size * normal_i + offset)));
    }
    for (i = normal_i; normal_i < i + 4; normal_i++) {
        vec3_set(
            0.0f, -1.0f, 0.0f, (float*)((*vertices)->data + (vert_size * normal_i + offset)));
    }
    for (i = normal_i; normal_i < i + 4; normal_i++) {
        vec3_set(
            0.0f, 0.0f, 1.0f, (float*)((*vertices)->data + (vert_size * normal_i + offset)));
    }
    for (i = normal_i; normal_i < i + 4; normal_i++) {
        vec3_set(
            0.0f, 0.0f, -1.0f, (float*)((*vertices)->data + (vert_size * normal_i + offset)));
    }

    /* init positions */

    /* +X face */
    offset = (*vertices)->attribute_offsets[TE_VA_POSITION];
    i = 0;
    vec3_set(half, -half, -half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(half, half, -half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(half, -half, half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(half, half, half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;

    /* -X face */
    vec3_set(-half, half, -half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(-half, -half, -half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(-half, half, half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(-half, -half, half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;

    /* +Y face */
    vec3_set(half, half, -half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(-half, half, -half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(half, half, half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(-half, half, half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;

    /* -Y face */
    vec3_set(-half, -half, -half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(half, -half, -half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(-half, -half, half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(half, -half, half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;

    /* +Z face */
    vec3_set(-half, -half, half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(half, -half, half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(-half, half, half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(half, half, half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;

    /* -Z face */
    vec3_set(-half, half, -half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(half, half, -half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(-half, -half, -half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;
    vec3_set(half, -half, -half, (float*)((*vertices)->data + (vert_size * i + offset)));
    i += 1;

    (*index_count) = 36;
    (*indices) = malloc(sizeof(unsigned short) * (*index_count));
    (*indices)[0] = 0; /* +X face */
    (*indices)[1] = 1;
    (*indices)[2] = 2;
    (*indices)[3] = 3;
    (*indices)[4] = 2;
    (*indices)[5] = 1;
    (*indices)[6] = 4; /* -X face */
    (*indices)[7] = 5;
    (*indices)[8] = 6;
    (*indices)[9] = 7;
    (*indices)[10] = 6;
    (*indices)[11] = 5;
    (*indices)[12] = 8; /* +Y face */
    (*indices)[13] = 9;
    (*indices)[14] = 10;
    (*indices)[15] = 11;
    (*indices)[16] = 10;
    (*indices)[17] = 9;
    (*indices)[18] = 12; /* -Y face */
    (*indices)[19] = 13;
    (*indices)[20] = 14;
    (*indices)[21] = 15;
    (*indices)[22] = 14;
    (*indices)[23] = 13;
    (*indices)[24] = 16; /* +Z face */
    (*indices)[25] = 17;
    (*indices)[26] = 18;
    (*indices)[27] = 19;
    (*indices)[28] = 18;
    (*indices)[29] = 17;
    (*indices)[30] = 20; /* -Z face */
    (*indices)[31] = 21;
    (*indices)[32] = 22;
    (*indices)[33] = 23;
    (*indices)[34] = 22;
    (*indices)[35] = 21;
}

void
mesh_generator_icosphere(
    struct te_vertex_pack** vertices, unsigned short** indices, unsigned int* index_count) {
    te_vec3 normal;
    te_vec2 uv;
    unsigned int i;
    unsigned int vert_size;
    unsigned char pos_offset;
    unsigned char norm_offset;
    unsigned char uv_offset;
    const float X = 0.525731112119133606f;
    const float Z = 0.850650808352039932f;
    const float N = 0.0f;

    te_vec3 positions[] = {{-X, N, Z}, {X, N, Z},  {-X, N, -Z}, {X, N, -Z},
                           {N, Z, X},  {N, Z, -X}, {N, -Z, X},  {N, -Z, -X},
                           {Z, X, N},  {-Z, X, N}, {Z, -X, N},  {-Z, -X, N}};

    unsigned short triangle_indices[] = {0, 4,  1,  0, 9, 4,  9, 5,  4, 4,  5, 8, 4, 8, 1,
                                         8, 10, 1,  8, 3, 10, 5, 3,  8, 5,  2, 3, 2, 7, 3,
                                         7, 10, 3,  7, 6, 10, 7, 11, 6, 11, 0, 6, 0, 1, 6,
                                         6, 1,  10, 9, 0, 11, 9, 11, 2, 9,  2, 5, 7, 2, 11};

    (*index_count) = 60;
    (*indices) = malloc(sizeof(unsigned short) * (*index_count));
    memcpy((*indices), triangle_indices, sizeof(unsigned short) * (*index_count));

    vec2_zero(uv);

    (*vertices) = vertex_pack_create(12, false);
    vert_size = (*vertices)->vertex_sizeof;
    pos_offset = (*vertices)->attribute_offsets[TE_VA_POSITION];
    norm_offset = (*vertices)->attribute_offsets[TE_VA_NORMAL];
    uv_offset = (*vertices)->attribute_offsets[TE_VA_UV];

    for (i = 0; i < 12; i++) {
        vec3_copy(positions[i], (float*)((*vertices)->data + (vert_size * i + pos_offset)));

        vec3_copy(positions[i], normal);
        vec3_normalize(normal);
        vec3_copy(normal, (float*)((*vertices)->data + (vert_size * i + norm_offset)));

        vec2_copy(uv, (float*)((*vertices)->data + (vert_size * i + uv_offset)));
    }
}
