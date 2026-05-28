#include "misc/mesh_generator.h"

#include <stdlib.h>
#include <string.h>
#include "game/model.h"

void mesh_generator_cube(
    struct te_vertex_pack** vertices, unsigned short** indices, unsigned int* index_count) {
    const float half = 0.5f;

    (*vertices) = vertex_pack_create(24, false);

    const unsigned int vert_size = (*vertices)->vertex_sizeof;

    // Init UVs.
    const unsigned char uv_offset = (*vertices)->attribute_offsets[TE_VA_UV];
    for (unsigned int i = 0; i < (*vertices)->vertex_count; i += 4) {
        glm_vec2_make(
            (vec2){1.0f, 1.0f}, (float*)((*vertices)->data + (vert_size * i + uv_offset)));
        glm_vec2_make(
            (vec2){0.0f, 1.0f},
            (float*)((*vertices)->data + (vert_size * (i + 1) + uv_offset)));
        glm_vec2_make(
            (vec2){1.0f, 0.0f},
            (float*)((*vertices)->data + (vert_size * (i + 2) + uv_offset)));
        glm_vec2_make(
            (vec2){0.0f, 0.0f},
            (float*)((*vertices)->data + (vert_size * (i + 3) + uv_offset)));
    }

    // Init normals.
    const unsigned char normal_offset = (*vertices)->attribute_offsets[TE_VA_NORMAL];
    unsigned int normal_i = 0;
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make(
            (vec3){1.0f, 0.0f, 0.0f},
            (float*)((*vertices)->data + (vert_size * normal_i + normal_offset)));
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make(
            (vec3){-1.0f, 0.0f, 0.0f},
            (float*)((*vertices)->data + (vert_size * normal_i + normal_offset)));
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make(
            (vec3){0.0f, 1.0f, 0.0f},
            (float*)((*vertices)->data + (vert_size * normal_i + normal_offset)));
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make(
            (vec3){0.0f, -1.0f, 0.0f},
            (float*)((*vertices)->data + (vert_size * normal_i + normal_offset)));
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make(
            (vec3){0.0f, 0.0f, 1.0f},
            (float*)((*vertices)->data + (vert_size * normal_i + normal_offset)));
    }
    for (unsigned int i = normal_i; normal_i < i + 4; normal_i++) {
        glm_vec3_make(
            (vec3){.0f, 0.0f, -1.0f},
            (float*)((*vertices)->data + (vert_size * normal_i + normal_offset)));
    }

    // Init positions.

    // +X face.
    const unsigned char pos_offset = (*vertices)->attribute_offsets[TE_VA_POSITION];
    unsigned int i = 0;
    glm_vec3_make(
        (vec3){half, -half, -half},
        (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){half, half, -half}, (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){half, -half, half}, (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){half, half, half}, (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;

    // -X face.
    glm_vec3_make(
        (vec3){-half, half, -half},
        (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){-half, -half, -half},
        (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){-half, half, half}, (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){-half, -half, half},
        (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;

    // +Y face.
    glm_vec3_make(
        (vec3){half, half, -half}, (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){-half, half, -half},
        (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){half, half, half}, (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){-half, half, half}, (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;

    // -Y face.
    glm_vec3_make(
        (vec3){-half, -half, -half},
        (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){half, -half, -half},
        (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){-half, -half, half},
        (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){half, -half, half}, (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;

    // +Z face.
    glm_vec3_make(
        (vec3){-half, -half, half},
        (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){half, -half, half}, (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){-half, half, half}, (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){half, half, half}, (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;

    // -Z face.
    glm_vec3_make(
        (vec3){-half, half, -half},
        (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){half, half, -half}, (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){-half, -half, -half},
        (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;
    glm_vec3_make(
        (vec3){half, -half, -half},
        (float*)((*vertices)->data + (vert_size * i + pos_offset)));
    i += 1;

    (*index_count) = 36;
    (*indices) = malloc(sizeof(unsigned short) * (*index_count));
    (*indices)[0] = 0; // +X face.
    (*indices)[1] = 1;
    (*indices)[2] = 2;
    (*indices)[3] = 3;
    (*indices)[4] = 2;
    (*indices)[5] = 1;
    (*indices)[6] = 4; // -X face.
    (*indices)[7] = 5;
    (*indices)[8] = 6;
    (*indices)[9] = 7;
    (*indices)[10] = 6;
    (*indices)[11] = 5;
    (*indices)[12] = 8; // +Y face.
    (*indices)[13] = 9;
    (*indices)[14] = 10;
    (*indices)[15] = 11;
    (*indices)[16] = 10;
    (*indices)[17] = 9;
    (*indices)[18] = 12; // -Y face.
    (*indices)[19] = 13;
    (*indices)[20] = 14;
    (*indices)[21] = 15;
    (*indices)[22] = 14;
    (*indices)[23] = 13;
    (*indices)[24] = 16; // +Z face.
    (*indices)[25] = 17;
    (*indices)[26] = 18;
    (*indices)[27] = 19;
    (*indices)[28] = 18;
    (*indices)[29] = 17;
    (*indices)[30] = 20; // -Z face.
    (*indices)[31] = 21;
    (*indices)[32] = 22;
    (*indices)[33] = 23;
    (*indices)[34] = 22;
    (*indices)[35] = 21;
}

void mesh_generator_icosphere(
    struct te_vertex_pack** vertices, unsigned short** indices, unsigned int* index_count) {
    const float X = 0.525731112119133606f;
    const float Z = 0.850650808352039932f;
    const float N = 0.0f;

    vec3 positions[] = {
        {-X, N, Z}, {X, N, Z},   {-X, N, -Z}, {X, N, -Z}, {N, Z, X},  {N, Z, -X},
        {N, -Z, X}, {N, -Z, -X}, {Z, X, N},   {-Z, X, N}, {Z, -X, N}, {-Z, -X, N}};

    unsigned short triangle_indices[] = {
        0, 4, 1, 0, 9, 4,  9, 5, 4,  4, 5, 8,  4, 8, 1,  8, 10, 1, 8, 3, 10,
        5, 3, 8, 5, 2, 3,  2, 7, 3,  7, 10, 3, 7, 6, 10, 7, 11, 6, 11, 0, 6,
        0, 1, 6, 6, 1, 10, 9, 0, 11, 9, 11, 2, 9, 2, 5,  7, 2, 11};

    (*index_count) = 60;
    (*indices) = malloc(sizeof(unsigned short) * (*index_count));
    memcpy((*indices), triangle_indices, sizeof(unsigned short) * (*index_count));

    vec2 uv;
    glm_vec2_zero(uv);

    (*vertices) = vertex_pack_create(12, false);
    const unsigned int vert_size = (*vertices)->vertex_sizeof;
    const unsigned char pos_offset = (*vertices)->attribute_offsets[TE_VA_POSITION];
    const unsigned char norm_offset = (*vertices)->attribute_offsets[TE_VA_NORMAL];
    const unsigned char uv_offset = (*vertices)->attribute_offsets[TE_VA_UV];

    for (unsigned int i = 0; i < 12; i++) {
        glm_vec3_copy(
            positions[i], (float*)((*vertices)->data + (vert_size * i + pos_offset)));

        vec3 normal;
        glm_vec3_copy(positions[i], normal);
        glm_normalize(normal);
        glm_vec3_copy(normal, (float*)((*vertices)->data + (vert_size * i + norm_offset)));

        glm_vec2_copy(uv, (float*)((*vertices)->data + (vert_size * i + uv_offset)));
    }
}