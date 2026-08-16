#pragma once

#include <stdbool.h>
#include <cglm/vec4.h>
#include <cglm/vec3.h>

typedef struct te_particle_emitter te_particle_emitter;

struct te_world;
struct te_game_object_info;

te_particle_emitter* particle_emitter_create(void);
void particle_emitter_destroy(te_particle_emitter* emitter);

// Returns game object info.
// Returned pointer is valid while the game object is valid.
struct te_game_object_info*
particle_emitter_get_game_object_info(te_particle_emitter* emitter);

// Optionally you can set a name of the emitter. The string will be copied.
// Returns NULL if was not set previously.
void particle_emitter_set_name(te_particle_emitter* emitter, const char* name);
const char* particle_emitter_get_name(te_particle_emitter* emitter);

void particle_emitter_set_position(te_particle_emitter* emitter, vec3 pos);
void particle_emitter_get_position(te_particle_emitter* emitter, vec3 out);

// Sets texture that particles will use.
void particle_emitter_set_texture(te_particle_emitter* emitter, const char* relative_path);
const char* particle_emitter_get_texture(te_particle_emitter* emitter);

// Sets RGBA color of particles.
void particle_emitter_set_color(te_particle_emitter* emitter, vec4 color);
void particle_emitter_get_color(te_particle_emitter* emitter, vec4 out);

// Sets RGBA color of particles during fade in (see @ref particle_emitter_set_fade_in_life_portion).
void particle_emitter_set_color_fade_in(te_particle_emitter* emitter, vec4 color);
void particle_emitter_get_color_fade_in(te_particle_emitter* emitter, vec4 out);

// Sets RGBA color of particles during fade out (see @ref particle_emitter_set_fade_out_life_portion).
void particle_emitter_set_color_fade_out(te_particle_emitter* emitter, vec4 color);
void particle_emitter_get_color_fade_out(te_particle_emitter* emitter, vec4 out);

// Sets velocity of particles when spawned.
void particle_emitter_set_spawn_velocity(te_particle_emitter* emitter, vec3 velocity);
void particle_emitter_get_spawn_velocity(te_particle_emitter* emitter, vec3 out);

// Sets a non-negative "range value" that will be randomly added to spawn velocity per particle.
// For example: for value 5 (on 1 axis) the spawn velocity will have a random value in range [-5; 5] added.
void particle_emitter_set_spawn_velocity_rand(te_particle_emitter* emitter, vec3 rand);
void particle_emitter_get_spawn_velocity_rand(te_particle_emitter* emitter, vec3 out);

// Sets a non-negative "range value" that will be randomly added to spawn position per particle.
// For example: for value 5 (on 1 axis) the spawn position will have a random value in range [-5; 5] added.
void particle_emitter_set_spawn_offset_rand(te_particle_emitter* emitter, vec3 rand);
void particle_emitter_get_spawn_offset_rand(te_particle_emitter* emitter, vec3 out);

// Sets gravity that affects particles
void particle_emitter_set_gravity(te_particle_emitter* emitter, vec3 gravity);
void particle_emitter_get_gravity(te_particle_emitter* emitter, vec3 out);

// Sets size of particles.
void particle_emitter_set_size(te_particle_emitter* emitter, float size);
float particle_emitter_get_size(te_particle_emitter* emitter);

// Sets size of particles during fade in (see @ref particle_emitter_set_fade_in_life_portion).
void particle_emitter_set_size_fade_in(te_particle_emitter* emitter, float size);
float particle_emitter_get_size_fade_in(te_particle_emitter* emitter);

// Sets size of particles during fade out (see @ref particle_emitter_set_fade_out_life_portion).
void particle_emitter_set_size_fade_out(te_particle_emitter* emitter, float size);
float particle_emitter_get_size_fade_out(te_particle_emitter* emitter);

// Sets time (in seconds) to live per particle.
void particle_emitter_set_time_to_live_sec(te_particle_emitter* emitter, float time_sec);
float particle_emitter_get_time_to_live_sec(te_particle_emitter* emitter);

// Sets time (in seconds) between particle spawns.
void particle_emitter_set_delay_between_spawns(te_particle_emitter* emitter, float time_sec);
float particle_emitter_get_delay_between_spawns(te_particle_emitter* emitter);

// Sets value in range [0.0; 1.0] relative to particle's time to live
// (see @ref particle_emitter_get_time_to_live_sec) that determines interval [0.0; X]
// during which fade in happens. Value of 0 disables fade in (instant fade in).
void particle_emitter_set_fade_in_life_portion(te_particle_emitter* emitter, float portion);
float particle_emitter_get_fade_in_life_portion(te_particle_emitter* emitter);

// Sets value in range [0.0; 1.0] relative to particle's time to live
// (see @ref particle_emitter_get_time_to_live_sec) that determines interval [X; 1.0]
// during which fade out happens. Value of 1 disables fade out (instant fade out).
void particle_emitter_set_fade_out_life_portion(te_particle_emitter* emitter, float portion);
float particle_emitter_get_fade_out_life_portion(te_particle_emitter* emitter);

// Can be used to pause emitting of new particles while keeping already existing particles simulated.
void particle_emitter_set_emit_new_particles(te_particle_emitter* emitter, bool emit);
bool particle_emitter_get_emit_new_particles(te_particle_emitter* emitter);

// Pauses the whole simulation. If you only need to stop emitting new particles use
// @ref particle_emitter_set_emit_new_particles.
void particle_emitter_set_is_paused(te_particle_emitter* emitter, bool is_paused);
bool particle_emitter_get_is_paused(te_particle_emitter* emitter);

// Allows disabling serialization of the particle emitter (enabled by default).
void particle_emitter_set_is_serialization_allowed(te_particle_emitter* emitter, bool enable);
bool particle_emitter_is_serialization_allowed(te_particle_emitter* emitter);

// Returns NULL if the emitter is not spawned.
// Do not free/destroy returned pointer, valid while the emitter exists.
struct te_world* particle_emitter_get_world(te_particle_emitter* emitter);

// Returns unique ID of this type in the type database.
const char* particle_emitter_get_type_id(void);
// Registers the type in the type database.
void particle_emitter_register_type(void);
