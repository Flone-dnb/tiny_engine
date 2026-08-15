#pragma once

#include <cglm/vec4.h>

typedef struct te_particle_renderer te_particle_renderer;
struct te_renderer;

// Data needed to render a single particle.
typedef struct te_particle_render_data {
    vec4 color;        // RGBA color
    vec4 pos_and_size; // pos in world space in XYZ and size in W
} te_particle_render_data;

// Data needed to render a bunch of particles.
typedef struct te_particle_emitter_render_data {
    te_particle_render_data* particles;
    unsigned int tex_id; // GL ID, 0 if not used
    unsigned int particle_count;
} te_particle_emitter_render_data;

te_particle_renderer* particle_renderer_create(struct te_renderer* renderer);
void particle_renderer_destroy(te_particle_renderer* renderer);

unsigned int particle_renderer_add_emitter(te_particle_renderer* renderer);
void particle_renderer_remove_emitter(te_particle_renderer* renderer, unsigned int handle);

// Never store/save pointer to render data because on the next frame
// the pointer may end up pointing to an invalid memory. Only use "get_render_data" function to quickly
// update some render data. Suffix "_tmp" is used because of this.
te_particle_emitter_render_data* particle_renderer_get_emitter_render_data_tmp(
    te_particle_renderer* renderer, unsigned int handle);

// Draws particles on the currently set framebuffer.
void particle_renderer_draw(te_particle_renderer* renderer, mat4* view_mat, mat4* proj_mat);
