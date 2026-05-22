#pragma once

struct te_vertex_pack;

// Generates a cube mesh, creates (allocates) vertices and indices, and returns pointers to allocated data.
void mesh_generator_cube(
    struct te_vertex_pack** vertices, unsigned short** indices, unsigned int* index_count);