#pragma once

struct te_vertex_pack;

/* generates a plane mesh, creates (allocates) vertices and indices, and returns pointers to allocated data */
void mesh_generator_plane(
    struct te_vertex_pack** vertices, unsigned short** indices, unsigned int* index_count);

/* generates a cube mesh, creates (allocates) vertices and indices, and returns pointers to allocated data */
void mesh_generator_cube(
    struct te_vertex_pack** vertices, unsigned short** indices, unsigned int* index_count);

/* generates an icosphere, creates (allocates) vertices and indices, and returns pointers to allocated data */
void mesh_generator_icosphere(
    struct te_vertex_pack** vertices, unsigned short** indices, unsigned int* index_count);
