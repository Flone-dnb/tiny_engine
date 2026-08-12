#include <sound_manager.h>

#include <stdlib.h>
#include <io/log.h>
#include <io/filesystem.h>
#include <miniaudio.h>

struct te_sound_manager {
    ma_engine ma_engine;
};

struct te_sound {
    // NULL if not set.
    void (*on_finished)(void* user_data, te_sound* sound);
    void* user_data;

    ma_sound ma_sound;
};

te_sound_manager*
sound_manager_create(void) {
    te_sound_manager* manager = malloc(sizeof(te_sound_manager));

    ma_result result = ma_engine_init(NULL, &manager->ma_engine);
    if (result != MA_SUCCESS) {
        log_error_fmt("failed to initialize miniaudio, error: %i", result);
        abort();
    }

    return manager;
}

void
sound_manager_destroy(te_sound_manager* sound_manager) {
    ma_engine_uninit(&sound_manager->ma_engine);

    free(sound_manager);
}

void
sound_manager_set_volume(te_sound_manager* sound_manager, float volume) {
    ma_engine_set_volume(&sound_manager->ma_engine, volume);
}

te_sound*
sound_create(te_sound_manager* sound_manager, const char* relative_path) {
    te_sound* sound = malloc(sizeof(te_sound));

    sound->on_finished = NULL;
    sound->user_data = NULL;

    char* path_to_sound = filesystem_prepend_res_to_path(relative_path, NULL);

    ma_result result = ma_sound_init_from_file(
        &sound_manager->ma_engine, path_to_sound,
        MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, NULL, &sound->ma_sound);
    if (result != MA_SUCCESS) {
        log_error_fmt(
            "failed to initialize sound from file %s, error: %i", path_to_sound, result);
        abort();
    }

    free(path_to_sound);

    return sound;
}

void
sound_destroy(te_sound* sound) {
    ma_sound_uninit(&sound->ma_sound);

    free(sound);
}

void
sound_play(te_sound* sound) {
    ma_result result = ma_sound_start(&sound->ma_sound);
    if (result != MA_SUCCESS) {
        log_error_fmt("failed to start sound, error: %i", result);
        abort();
    }
}

void
sound_stop(te_sound* sound) {
    ma_result result = ma_sound_stop(&sound->ma_sound);
    if (result != MA_SUCCESS) {
        log_error_fmt("failed to stop sound, error: %i", result);
        abort();
    }
}

bool
sound_is_playing(te_sound* sound) {
    return ma_sound_is_playing(&sound->ma_sound) != 0;
}

void
sound_set_volume(te_sound* sound, float volume) {
    ma_sound_set_volume(&sound->ma_sound, volume);
}

void
sound_set_is_looping(te_sound* sound, bool is_looping) {
    ma_sound_set_looping(&sound->ma_sound, is_looping);
}

void
sound_set_pan(te_sound* sound, float pan) {
    ma_sound_set_pan(&sound->ma_sound, pan);
}

void
sound_set_pitch(te_sound* sound, float pitch) {
    ma_sound_set_pitch(&sound->ma_sound, pitch);
}

bool
sound_is_finished_playing(te_sound* sound) {
    return ma_sound_at_end(&sound->ma_sound);
}

void
sound_set_3d_position(te_sound* sound, vec3 pos) {
    ma_sound_set_spatialization_enabled(
        &sound->ma_sound, 1); // because we disable it by default

    ma_sound_set_position(&sound->ma_sound, pos[0], pos[1], pos[2]);
}

void
sound_set_distance(te_sound* sound, float min, float max) {
    ma_sound_set_spatialization_enabled(
        &sound->ma_sound, 1); // because we disable it by default

    ma_sound_set_min_distance(&sound->ma_sound, min);
    ma_sound_set_max_distance(&sound->ma_sound, max);
}

void
prv_sound_manager_set_listener(
    te_sound_manager* sound_manager, vec3 pos, vec3 forward, vec3 up) {
    ma_engine_listener_set_position(&sound_manager->ma_engine, 0, pos[0], pos[1], pos[2]);
    ma_engine_listener_set_direction(
        &sound_manager->ma_engine, 0, forward[0], forward[1], forward[2]);
    ma_engine_listener_set_world_up(&sound_manager->ma_engine, 0, up[0], up[1], up[2]);
}

static void
on_sound_end(void* user_data, ma_sound* ma_sound) {
    (void)ma_sound;

    te_sound* sound = user_data;

    sound->on_finished(sound->user_data, sound);
}
void
prv_sound_set_on_finished_callback_audio_thread(
    te_sound* sound, void* user_data, void (*on_finished)(void* user_data, te_sound* sound)) {
    sound->user_data = user_data;
    sound->on_finished = on_finished;
    ma_result result = ma_sound_set_end_callback(&sound->ma_sound, on_sound_end, sound);
    if (result != MA_SUCCESS) {
        log_error_fmt("failed to set sound end callback, error: %i", result);
        abort();
    }
}
