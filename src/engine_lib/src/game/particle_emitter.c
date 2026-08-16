#include <game/particle_emitter.h>

#include <stdio.h>
#include <world.h>
#include <type_database.h>
#include <game/game_object_info.h>
#include <game_manager.h>
#include <render/particle_renderer.h>
#include <render/texture_manager.h>
#include <render/renderer.h>
#include <io/filesystem.h>
#include <io/log.h>
#include <time.h>

#define PARTICLE_EMITTER_TEX_LOAD_OPTION TE_TLO_GENERATE_MIPMAPS

// Data used to simulate a single particle.
typedef struct te_particle_data {
    vec4 color;

    vec3 pos;
    float size;

    vec3 velocity;
    float left_time_to_live_sec;
} te_particle_data;

struct te_particle_emitter {
    te_game_object_info* game_object_info;

    char* tex_relative_path;
    char* name;

    te_world* world;

    // Used during simulation.
    te_particle_data* particle_buf_a;
    te_particle_data* particle_buf_b;

    vec4 color;

    vec4 color_fade_in;
    vec4 color_fade_out;

    vec3 position;

    vec3 spawn_velocity;
    vec3 spawn_velocity_rand;
    vec3 spawn_offset_rand;

    vec3 gravity;

    float size;
    float size_fade_in;
    float size_fade_out;

    float time_to_live_sec;
    float delay_between_spawns;

    float fade_in_life_portion;
    float fade_out_life_portion;

    // Used during simulation. Time (in seconds) until delay between spawns finishes.
    float transient_time_before_new_spawn;

    // 0 if not bound.
    unsigned int tex_id;

    // 0xFFFFFFFF if not being rendered.
    unsigned int render_data_handle;

    // ID used to unregister tick callback. 0xFFFFFFFF if not registered.
    unsigned int tick_callback_id;

    // Size of render data buffers, calculated after spawned.
    unsigned int transient_max_particle_count;

    // Used during simulation.
    unsigned int transient_alive_particle_count;

    bool is_paused;
    bool emit_new_particles;
    bool is_buf_a;
    bool is_serialization_allowed;
};

static void on_spawned(te_particle_emitter* emitter, te_world* world);
static void on_despawned(te_particle_emitter* emitter);

te_particle_emitter*
particle_emitter_create(void) {
    te_particle_emitter* emitter = malloc(sizeof(te_particle_emitter));

    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, emitter->color);
    glm_vec4_copy(emitter->color, emitter->color_fade_in);
    glm_vec4_copy(emitter->color, emitter->color_fade_out);

    glm_vec3_copy((vec3){0.0f, 2.0f, 0.0f}, emitter->spawn_velocity);
    glm_vec3_copy((vec3){0.5f, 0.0f, 0.5f}, emitter->spawn_velocity_rand);
    glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, emitter->spawn_offset_rand);

    glm_vec3_copy((vec3){0.0f, -1.0f, 0.0f}, emitter->gravity);

    glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, emitter->position);

    emitter->size = 0.25f;
    emitter->size_fade_in = emitter->size;
    emitter->size_fade_out = emitter->size;

    emitter->time_to_live_sec = 2.0f;
    emitter->delay_between_spawns = 0.25f;

    emitter->fade_in_life_portion = 0.0f;
    emitter->fade_out_life_portion = 1.0f;

    emitter->tick_callback_id = 0xFFFFFFFF;
    emitter->render_data_handle = 0xFFFFFFFF;
    emitter->tex_id = 0;
    emitter->transient_max_particle_count = 0;
    emitter->transient_alive_particle_count = 0;

    emitter->is_paused = false;
    emitter->is_serialization_allowed = true;
    emitter->emit_new_particles = true;

    emitter->world = NULL;
    emitter->name = NULL;
    emitter->particle_buf_a = NULL;
    emitter->particle_buf_b = NULL;
    emitter->is_buf_a = true;
    emitter->tex_relative_path = NULL;

    emitter->game_object_info = malloc(sizeof(te_game_object_info));
    emitter->game_object_info->type_id = particle_emitter_get_type_id();
    emitter->game_object_info->type = TE_GOT_PARTICLE_EMITTER;
    emitter->game_object_info->game_object = emitter;
    emitter->game_object_info->get_world = particle_emitter_get_world;
    emitter->game_object_info->get_name = particle_emitter_get_name;
    emitter->game_object_info->on_spawned = on_spawned;
    emitter->game_object_info->on_despawned = on_despawned;
    emitter->game_object_info->destroy = particle_emitter_destroy;

    return emitter;
}

void
particle_emitter_destroy(te_particle_emitter* emitter) {
    free(emitter->name);
    free(emitter->game_object_info);
    free(emitter->tex_relative_path);

    free(emitter);
}

te_game_object_info*
particle_emitter_get_game_object_info(te_particle_emitter* emitter) {
    return emitter->game_object_info;
}

void
particle_emitter_set_name(te_particle_emitter* emitter, const char* name) {
    free(emitter->name);
    emitter->name = NULL;

    if (name != NULL) {
        const size_t len = strlen(name);
        emitter->name = malloc(sizeof(char) * (len + 1));
        memcpy(emitter->name, name, sizeof(char) * len);
        emitter->name[len] = 0;
    }
}

const char*
particle_emitter_get_name(te_particle_emitter* emitter) {
    return emitter->name;
}

void
particle_emitter_set_position(te_particle_emitter* emitter, vec3 pos) {
    glm_vec3_copy(pos, emitter->position);
}

void
particle_emitter_get_position(te_particle_emitter* emitter, vec3 out) {
    glm_vec3_copy(emitter->position, out);
}

#if defined(ENGINE_EDITOR)
static bool
check_file_path(const char* relative_path) {
    if (relative_path == NULL) {
        return true;
    }

    if (relative_path != NULL) {
        // Check if path exists.
        char* res_path = filesystem_prepend_res_to_path(relative_path, NULL);
        if (!filesystem_does_path_exists(res_path)) {
            // Do nothing, probably user typing the path.
            free(res_path);
            return false;
        }
        FILE* fp = fopen(res_path, "rb");
        if (fp == NULL) {
            // Not a file.
            free(res_path);
            return false;
        }
        fclose(fp);
        free(res_path);
    }

    return true;
}
#endif

void
particle_emitter_set_texture(te_particle_emitter* emitter, const char* relative_path) {
#if defined(ENGINE_EDITOR)
    if (!check_file_path(relative_path)) {
        return;
    }
#endif

    free(emitter->tex_relative_path);
    emitter->tex_relative_path = NULL;

    if (relative_path == NULL || strcmp(relative_path, "") == 0) {
        // Remove current texture.
        emitter->tex_relative_path = NULL;
        if (emitter->world != NULL) {
            // Update render data.
            te_particle_renderer* particle_renderer =
                world_get_particle_renderer(emitter->world);
            te_particle_emitter_render_data* data =
                particle_renderer_get_emitter_render_data_tmp(
                    particle_renderer, emitter->render_data_handle);
            data->tex_id = 0;

            if (emitter->tex_id > 0) {
                te_texture_manager* texture_manager = renderer_get_texture_manager(
                    game_manager_get_renderer(world_get_game_manager(emitter->world)));
                texture_manager_mark_unused_texture(texture_manager, emitter->tex_id);
            }

            emitter->tex_id = 0;
            data->tex_id = 0;
        }
    } else {
        // Set new texture.
        const size_t len = strlen(relative_path);
        emitter->tex_relative_path = malloc(sizeof(char) * (len + 1));
        memcpy(emitter->tex_relative_path, relative_path, sizeof(char) * len);
        emitter->tex_relative_path[len] = 0;

        if (emitter->world != NULL) {
            te_texture_manager* texture_manager = renderer_get_texture_manager(
                game_manager_get_renderer(world_get_game_manager(emitter->world)));
            if (emitter->tex_id > 0) {
                texture_manager_mark_unused_texture(texture_manager, emitter->tex_id);
            }
            emitter->tex_id = texture_manager_request_texture(
                texture_manager, relative_path, PARTICLE_EMITTER_TEX_LOAD_OPTION);

            // Update render data.
            te_particle_renderer* particle_renderer =
                world_get_particle_renderer(emitter->world);
            te_particle_emitter_render_data* data =
                particle_renderer_get_emitter_render_data_tmp(
                    particle_renderer, emitter->render_data_handle);
            data->tex_id = emitter->tex_id;
        }
    }
}

const char*
particle_emitter_get_texture(te_particle_emitter* emitter) {
    return emitter->tex_relative_path;
}

void
particle_emitter_set_color(te_particle_emitter* emitter, vec4 color) {
    glm_vec4_copy(color, emitter->color);
}

void
particle_emitter_get_color(te_particle_emitter* emitter, vec4 out) {
    glm_vec4_copy(emitter->color, out);
}

void
particle_emitter_set_color_fade_in(te_particle_emitter* emitter, vec4 color) {
    glm_vec4_copy(color, emitter->color_fade_in);
}

void
particle_emitter_get_color_fade_in(te_particle_emitter* emitter, vec4 out) {
    glm_vec4_copy(emitter->color_fade_in, out);
}

void
particle_emitter_set_color_fade_out(te_particle_emitter* emitter, vec4 color) {
    glm_vec4_copy(color, emitter->color_fade_out);
}

void
particle_emitter_get_color_fade_out(te_particle_emitter* emitter, vec4 out) {
    glm_vec4_copy(emitter->color_fade_out, out);
}

void
particle_emitter_set_spawn_velocity(te_particle_emitter* emitter, vec3 velocity) {
    glm_vec3_copy(velocity, emitter->spawn_velocity);
}

void
particle_emitter_get_spawn_velocity(te_particle_emitter* emitter, vec3 out) {
    glm_vec3_copy(emitter->spawn_velocity, out);
}

void
particle_emitter_set_spawn_velocity_rand(te_particle_emitter* emitter, vec3 rand) {
    glm_vec3_abs(rand, rand);
    glm_vec3_copy(rand, emitter->spawn_velocity_rand);
}

void
particle_emitter_get_spawn_velocity_rand(te_particle_emitter* emitter, vec3 out) {
    glm_vec3_copy(emitter->spawn_velocity_rand, out);
}

void
particle_emitter_set_spawn_offset_rand(te_particle_emitter* emitter, vec3 rand) {
    glm_vec3_abs(rand, rand);
    glm_vec3_copy(rand, emitter->spawn_offset_rand);
}

void
particle_emitter_get_spawn_offset_rand(te_particle_emitter* emitter, vec3 out) {
    glm_vec3_copy(emitter->spawn_offset_rand, out);
}

void
particle_emitter_set_gravity(te_particle_emitter* emitter, vec3 gravity) {
    glm_vec3_copy(gravity, emitter->gravity);
}

void
particle_emitter_get_gravity(te_particle_emitter* emitter, vec3 out) {
    glm_vec3_copy(emitter->gravity, out);
}

void
particle_emitter_set_size(te_particle_emitter* emitter, float size) {
    emitter->size = size;
}

float
particle_emitter_get_size(te_particle_emitter* emitter) {
    return emitter->size;
}

void
particle_emitter_set_size_fade_in(te_particle_emitter* emitter, float size) {
    emitter->size_fade_in = size;
}

float
particle_emitter_get_size_fade_in(te_particle_emitter* emitter) {
    return emitter->size_fade_in;
}

void
particle_emitter_set_size_fade_out(te_particle_emitter* emitter, float size) {
    emitter->size_fade_out = size;
}

float
particle_emitter_get_size_fade_out(te_particle_emitter* emitter) {
    return emitter->size_fade_out;
}

static void add_to_renderer(te_particle_emitter* emitter);
static void remove_from_renderer(te_particle_emitter* emitter);

void
particle_emitter_set_time_to_live_sec(te_particle_emitter* emitter, float time_sec) {
    emitter->time_to_live_sec = glm_max(time_sec, 0.001f);

    if (emitter->world != NULL) {
        remove_from_renderer(emitter);
        add_to_renderer(emitter);
    }
}

float
particle_emitter_get_time_to_live_sec(te_particle_emitter* emitter) {
    return emitter->time_to_live_sec;
}

void
particle_emitter_set_delay_between_spawns(te_particle_emitter* emitter, float time_sec) {
    emitter->delay_between_spawns = glm_max(time_sec, 0.001f);

    if (emitter->world != NULL) {
        remove_from_renderer(emitter);
        add_to_renderer(emitter);
    }
}

float
particle_emitter_get_delay_between_spawns(te_particle_emitter* emitter) {
    return emitter->delay_between_spawns;
}

void
particle_emitter_set_fade_in_life_portion(te_particle_emitter* emitter, float portion) {
    emitter->fade_in_life_portion = glm_clamp(portion, 0.0f, 1.0f);
}

float
particle_emitter_get_fade_in_life_portion(te_particle_emitter* emitter) {
    return emitter->fade_in_life_portion;
}

void
particle_emitter_set_fade_out_life_portion(te_particle_emitter* emitter, float portion) {
    emitter->fade_out_life_portion = glm_clamp(portion, 0.0f, 1.0f);
}

float
particle_emitter_get_fade_out_life_portion(te_particle_emitter* emitter) {
    return emitter->fade_out_life_portion;
}

void
particle_emitter_set_emit_new_particles(te_particle_emitter* emitter, bool emit) {
    emitter->emit_new_particles = emit;
}

bool
particle_emitter_get_emit_new_particles(te_particle_emitter* emitter) {
    return emitter->emit_new_particles;
}

void
particle_emitter_set_is_paused(te_particle_emitter* emitter, bool is_paused) {
    emitter->is_paused = is_paused;
}

bool
particle_emitter_get_is_paused(te_particle_emitter* emitter) {
    return emitter->is_paused;
}

void
particle_emitter_set_is_serialization_allowed(te_particle_emitter* emitter, bool enable) {
    emitter->is_serialization_allowed = enable;
}

te_world*
particle_emitter_get_world(te_particle_emitter* emitter) {
    return emitter->world;
}

bool
particle_emitter_is_serialization_allowed(te_particle_emitter* emitter) {
    return emitter->is_serialization_allowed;
}

const char*
particle_emitter_get_type_id(void) {
    return "particle_emitter";
}

static void
type_spawn(te_world* world, te_particle_emitter* emitter) {
    if (emitter->world != NULL) {
        log_error("the particle emitter is already spawned in the different world");
        abort();
    }

    world_spawn_game_object(world, emitter->game_object_info);
}

static void
type_despawn(te_world* world, te_particle_emitter* emitter) {
    if (emitter->world != world) {
        log_error("the particle emitter is spawned in the different world");
        abort();
    }

    world_despawn_game_object(emitter->world, emitter->game_object_info);
}

void
particle_emitter_register_type(void) {
    te_type_info* info = type_info_create(
        particle_emitter_get_type_id(), particle_emitter_create, particle_emitter_destroy,
        type_spawn, type_despawn, NULL, particle_emitter_get_game_object_info,
        particle_emitter_is_serialization_allowed);

    type_info_add_vec3_variable(
        info, "position", particle_emitter_set_position, particle_emitter_get_position);

    type_info_add_string_variable(
        info, "texture", particle_emitter_set_texture, particle_emitter_get_texture);

    type_info_add_vec4_variable(
        info, "color", particle_emitter_set_color, particle_emitter_get_color);
    type_info_add_vec4_variable(
        info, "color_fade_in", particle_emitter_set_color_fade_in,
        particle_emitter_get_color_fade_in);
    type_info_add_vec4_variable(
        info, "color_fade_out", particle_emitter_set_color_fade_out,
        particle_emitter_get_color_fade_out);

    type_info_add_vec3_variable(
        info, "spawn_velocity", particle_emitter_set_spawn_velocity,
        particle_emitter_get_spawn_velocity);
    type_info_add_vec3_variable(
        info, "spawn_velocity_rand", particle_emitter_set_spawn_velocity_rand,
        particle_emitter_get_spawn_velocity_rand);
    type_info_add_vec3_variable(
        info, "spawn_offset_rand", particle_emitter_set_spawn_offset_rand,
        particle_emitter_get_spawn_offset_rand);

    type_info_add_vec3_variable(
        info, "gravity", particle_emitter_set_gravity, particle_emitter_get_gravity);

    type_info_add_float_variable(
        info, "size", particle_emitter_set_size, particle_emitter_get_size);
    type_info_add_float_variable(
        info, "size_fade_in", particle_emitter_set_size_fade_in,
        particle_emitter_get_size_fade_in);
    type_info_add_float_variable(
        info, "size_fade_out", particle_emitter_set_size_fade_out,
        particle_emitter_get_size_fade_out);

    type_info_add_float_variable(
        info, "time_to_live_sec", particle_emitter_set_time_to_live_sec,
        particle_emitter_get_time_to_live_sec);
    type_info_add_float_variable(
        info, "delay_between_spawns", particle_emitter_set_delay_between_spawns,
        particle_emitter_get_delay_between_spawns);

    type_info_add_float_variable(
        info, "fade_in_life_portion", particle_emitter_set_fade_in_life_portion,
        particle_emitter_get_fade_in_life_portion);
    type_info_add_float_variable(
        info, "fade_out_life_portion", particle_emitter_set_fade_out_life_portion,
        particle_emitter_get_fade_out_life_portion);

    type_info_add_bool_variable(
        info, "is_paused", particle_emitter_set_is_paused, particle_emitter_get_is_paused);

    type_info_add_string_variable(
        info, "name", particle_emitter_set_name, particle_emitter_get_name);

    type_database_register_type(info);
}

static inline void
get_vec3_with_rand(vec3 base, vec3 rnd, vec3 out) {
    float x = ((((float)rand() / (float)(RAND_MAX)) - 0.5f) * 2.0f) * rnd[0];
    float y = ((((float)rand() / (float)(RAND_MAX)) - 0.5f) * 2.0f) * rnd[1];
    float z = ((((float)rand() / (float)(RAND_MAX)) - 0.5f) * 2.0f) * rnd[2];
    glm_vec3_add(base, (vec3){x, y, z}, out);
}

static void
emitter_tick(te_particle_emitter* emitter, float delta_time_sec) {
    if (emitter->is_paused) {
        return;
    }

    te_particle_data* from = NULL;
    te_particle_data* to = NULL;
    if (emitter->is_buf_a) {
        from = emitter->particle_buf_a;
        to = emitter->particle_buf_b;
    } else {
        from = emitter->particle_buf_b;
        to = emitter->particle_buf_a;
    }

    unsigned int new_particle_count = 0;
    vec3 temp3;
    for (unsigned int i = 0; i < emitter->transient_alive_particle_count; i++) {
        te_particle_data* data = &from[i];

        // Update TTL.
        data->left_time_to_live_sec -= delta_time_sec;
        if (data->left_time_to_live_sec <= 0.0f) {
            continue;
        }

        // Update position.
        glm_vec3_mul(
            data->velocity, (vec3){delta_time_sec, delta_time_sec, delta_time_sec}, temp3);
        glm_vec3_add(data->pos, temp3, data->pos);

        // Update velocity.
        glm_vec3_mul(
            emitter->gravity, (vec3){delta_time_sec, delta_time_sec, delta_time_sec}, temp3);
        glm_vec3_add(data->velocity, temp3, data->velocity);

        float life_portion = 1.0f - (data->left_time_to_live_sec / emitter->time_to_live_sec);
        float fade_in_portion =
            1.0f - glm_smoothstep(0.0f, emitter->fade_in_life_portion, life_portion);
        float fade_out_portion =
            glm_smoothstep(emitter->fade_out_life_portion, 1.0f, life_portion);

        // Update color.
        glm_vec4_copy(emitter->color, data->color);
        glm_vec4_lerp(data->color, emitter->color_fade_in, fade_in_portion, data->color);
        glm_vec4_lerp(data->color, emitter->color_fade_out, fade_out_portion, data->color);

        // Update size.
        data->size = emitter->size;
        data->size = glm_lerp(data->size, emitter->size_fade_in, fade_in_portion);
        data->size = glm_lerp(data->size, emitter->size_fade_out, fade_out_portion);

        memcpy(&to[new_particle_count], data, sizeof(te_particle_data));
        new_particle_count += 1;
    }

    if (emitter->emit_new_particles) {
        emitter->transient_time_before_new_spawn -= delta_time_sec;
        if (emitter->transient_time_before_new_spawn <= 0.0f) {
            emitter->transient_time_before_new_spawn = emitter->delay_between_spawns;

            // Emit a single particle.
            te_particle_data* data = &to[new_particle_count];
            new_particle_count += 1;

            // Init particle.
            get_vec3_with_rand(emitter->position, emitter->spawn_offset_rand, data->pos);
            get_vec3_with_rand(
                emitter->spawn_velocity, emitter->spawn_velocity_rand, data->velocity);
            data->left_time_to_live_sec = emitter->time_to_live_sec;
            if (emitter->fade_in_life_portion > 0.0f) {
                glm_vec4_copy(emitter->color_fade_in, data->color);
                data->size = emitter->size_fade_in;
            } else {
                glm_vec4_copy(emitter->color, data->color);
                data->size = emitter->size;
            }
        }
    }

    emitter->transient_alive_particle_count = new_particle_count;
    emitter->is_buf_a = !emitter->is_buf_a;

    // Update render data.
    te_particle_renderer* particle_renderer = world_get_particle_renderer(emitter->world);
    te_particle_emitter_render_data* data = particle_renderer_get_emitter_render_data_tmp(
        particle_renderer, emitter->render_data_handle);

    data->particle_count = emitter->transient_alive_particle_count;
    for (unsigned int i = 0; i < data->particle_count; i++) {
        te_particle_data* src = &to[i];
        te_particle_render_data* dst = &data->particles[i];

        glm_vec4_copy(src->color, dst->color);
        glm_vec3_copy(src->pos, dst->pos_and_size);
        dst->pos_and_size[3] = src->size;
    }
}

static void
add_to_renderer(te_particle_emitter* emitter) {
    if (emitter->tex_relative_path != NULL) {
        te_texture_manager* texture_manager = renderer_get_texture_manager(
            game_manager_get_renderer(world_get_game_manager(emitter->world)));

        emitter->tex_id = texture_manager_request_texture(
            texture_manager, emitter->tex_relative_path, PARTICLE_EMITTER_TEX_LOAD_OPTION);
    }

    te_particle_renderer* particle_renderer = world_get_particle_renderer(emitter->world);
    emitter->render_data_handle = particle_renderer_add_emitter(particle_renderer);

    te_particle_emitter_render_data* data = particle_renderer_get_emitter_render_data_tmp(
        particle_renderer, emitter->render_data_handle);
    data->tex_id = emitter->tex_id;
    data->particle_count = 0;
    data->particles = NULL;

    // Estimate max particle count for GPU data.
    emitter->transient_max_particle_count = (unsigned int)ceilf(
        ceilf(emitter->time_to_live_sec + 0.1f) / emitter->delay_between_spawns);
    data->particles =
        malloc(sizeof(te_particle_render_data) * emitter->transient_max_particle_count);

    emitter->particle_buf_a =
        malloc(sizeof(te_particle_data) * emitter->transient_max_particle_count);
    emitter->particle_buf_b =
        malloc(sizeof(te_particle_data) * emitter->transient_max_particle_count);

    emitter->transient_alive_particle_count = 0;
    emitter->transient_time_before_new_spawn = 0.0f; // ignore spawn delay on first tick
    emitter->is_buf_a = true;
}

static void
remove_from_renderer(te_particle_emitter* emitter) {
    te_particle_renderer* particle_renderer = world_get_particle_renderer(emitter->world);

    te_particle_emitter_render_data* data = particle_renderer_get_emitter_render_data_tmp(
        particle_renderer, emitter->render_data_handle);
    free(data->particles);

    particle_renderer_remove_emitter(particle_renderer, emitter->render_data_handle);
    emitter->render_data_handle = 0xFFFFFFFF;

    free(emitter->particle_buf_a);
    free(emitter->particle_buf_b);
    emitter->particle_buf_a = NULL;
    emitter->particle_buf_b = NULL;

    emitter->transient_max_particle_count = 0;
    emitter->transient_alive_particle_count = 0;
    emitter->is_buf_a = true;

    if (emitter->tex_id > 0) {
        te_texture_manager* texture_manager = renderer_get_texture_manager(
            game_manager_get_renderer(world_get_game_manager(emitter->world)));
        texture_manager_mark_unused_texture(texture_manager, emitter->tex_id);

        emitter->tex_id = 0;
    }
}

static void
on_spawned(te_particle_emitter* emitter, te_world* world) {
    emitter->world = world;
    add_to_renderer(emitter);

    emitter->tick_callback_id = game_manager_add_tick_callback(
        world_get_game_manager(emitter->world), emitter, emitter_tick);
}

static void
on_despawned(te_particle_emitter* emitter) {
    game_manager_remove_tick_callback(
        world_get_game_manager(emitter->world), emitter->tick_callback_id);
    emitter->tick_callback_id = 0xFFFFFFFF;

    remove_from_renderer(emitter);
    emitter->world = NULL;
}
