#if defined(__linux__)
/* for time */
#define _POSIX_C_SOURCE 199309L
#endif

#include <sound_manager.h>

#include <stdlib.h>
#include <stdio.h>
#include <io/log.h>
#include <io/filesystem.h>
#include <sound/audio_mixer.h>
#include <thread/thread.h>
#include <thread/mutex.h>
#include <misc/globals.h>
#include <sound/sound_constants.h>

#if defined(ENGINE_DEBUG_TOOLS)
#include <debug_console.h>
#endif

#if defined(WIN32)
#define WIN32_MEAN_AND_LEAN
#define NOMINMAX
#include <Windows.h>
#elif defined(__linux__)
#include <sched.h>
#include <time.h>
#include <errno.h>
#else
#error "unsupported OS"
#endif

#if defined(ENGINE_32BIT)
#define MINIMP3_NO_SIMD
#endif
#define MINIMP3_IMPLEMENTATION
#include <minimp3.h>

/* minimp3 recommends having as many as 10 consecutive MP3 frames (~16KB) in the input buffer */
#define TE_SM_MP3_SOURCE_BUFFER_REQUIRED_SIZE 1024 * 16

/* plays 1 sound file */
typedef struct te_audio_thread {
    /* always valid */
    mp3dec_t* mp3d;
    te_thread* thread;
    te_sound* sound;

    /* includes `res` directory, must be freed */
    char* path_to_audio;

    /* true if being destroyed and need to end the thread */
    bool exit_thread;

    bool is_paused;
    bool restart;
} te_audio_thread;

struct te_sound_manager {
    /* NULL if nothing is playing (includes paused sounds (not stopped)) */
    te_audio_thread** audio_threads;

    /* always valid */
    te_mutex* mtx_audio_threads;
    te_audio_mixer* mixer;

    te_vec3 listener_pos;
    te_vec3 listener_right;

    unsigned int audio_thread_count;
    unsigned int audio_thread_capacity;
};

struct te_sound {
    /* NULL if not set */
    void (*on_finished)(void* user_data, te_sound* sound);
    void* user_data;

    /* always valid */
    te_sound_manager* sm;
    const char* relative_path_to_audio;

    /* NULL if was never started yet or was stopped (not paused), associated audio thread that plays it
     * this pointer is also audio source ID in the audio mixer */
    te_audio_thread* audio_thread;

    /* for 3D sound */
    te_vec3 pos;

    float volume;             /* user controlled */
    float volume_attenuation; /* used for distance attenuation */
    float pan;                /* user controlled but also used for 3d spatialization */
    float attenuation_distance;

    bool is_finished_playing;
    bool loop;
    bool spatial; /* false if 2D */
};

static te_audio_thread* create_audio_thread_locked(te_sound* sound);
static void destroy_audio_thread_locked(te_sound* sound);

static void init_audio_decoding_thread(
    te_audio_thread* t, te_audio_mixer* mixer, te_sound* sound,
    const char* relative_path_to_audio);
static void deinit_audio_decoding_thread(te_sound* sound);

te_sound_manager*
sound_manager_create(void) {
    te_sound_manager* manager = malloc(sizeof(te_sound_manager));

    manager->audio_threads = NULL;
    manager->audio_thread_count = 0;
    manager->audio_thread_capacity = 0;
    manager->mtx_audio_threads = mutex_create();
    manager->mixer = audio_mixer_create();

    vec3_zero(manager->listener_pos);
    globals_get_world_right(manager->listener_right);

    return manager;
}

void
sound_manager_destroy(te_sound_manager* sm) {
    mutex_lock(sm->mtx_audio_threads);
    if (sm->audio_thread_count > 0) {
        /* all sounds (and threads) should have been destroyed by now */
        log_error(
            __FILE__, __LINE__,
            "sound manager is being destroyed but there are still some sounds playing");
    }
    mutex_unlock(sm->mtx_audio_threads);

    mutex_destroy(sm->mtx_audio_threads);

    audio_mixer_destroy(sm->mixer);

    free(sm);
}

static void
yield_thread(void) {
#if defined(WIN32)
    Sleep(0);
#elif defined(__linux__)
    sched_yield();
#endif
}

static void
sleep_ms(unsigned int ms) {
#if defined(WIN32)
    Sleep(ms);
#elif defined(__linux__)
    struct timespec requested;
    struct timespec remaining;
    requested.tv_sec = 0;
    requested.tv_nsec = ms * 1000000;
    while (nanosleep(&requested, &remaining) == -1) {
        if (errno == EINTR) {
            requested = remaining;
        } else {
            break;
        }
    }
#endif
}

static void
audio_decoding_thread(void* userdata) {
    te_audio_thread* t;
    short* pcm;
    char* input_buffer;
    te_audio_mixer* mixer;
    FILE* fp;
    mp3dec_frame_info_t frame_info;
    size_t input_buffer_size, input_processed_bytes;
    unsigned long bytes_left, file_size;
    int sample_count;
    bool did_first_read;

    pcm = malloc(sizeof(short) * MINIMP3_MAX_SAMPLES_PER_FRAME);
    input_buffer = malloc(TE_SM_MP3_SOURCE_BUFFER_REQUIRED_SIZE * 2);
    input_buffer_size = TE_SM_MP3_SOURCE_BUFFER_REQUIRED_SIZE * 2;

    t = userdata;
    mixer = t->sound->sm->mixer;

    fp = fopen(t->path_to_audio, "rb");
    if (fp == NULL) {
        t->exit_thread = true;
        goto audio_decoding_thread_cleanup;
    }

    /* get file size */
    fseek(fp, 0, SEEK_END);
    {
        long size = ftell(fp);
        if (size < 0) {
            t->exit_thread = true;
            goto audio_decoding_thread_cleanup;
        }
        file_size = (unsigned long)size;
    }

    do {
        if (t->restart) {
            t->restart = false;
        }
        rewind(fp);
        bytes_left = file_size;

        input_processed_bytes = 0;
        did_first_read = false;
        audio_mixer_set_audio_source_wont_have_new_samples(mixer, t, false);

        while (!t->exit_thread && !t->restart) {
            if (bytes_left == 0) {
                if (input_processed_bytes + TE_SM_MP3_SOURCE_BUFFER_REQUIRED_SIZE / 4
                    >= input_buffer_size) {
                    break;
                }
            }
            if (t->is_paused) {
                yield_thread();
                continue;
            }

            if (bytes_left > 0
                && (!did_first_read
                    || (input_buffer_size - input_processed_bytes
                        < TE_SM_MP3_SOURCE_BUFFER_REQUIRED_SIZE))) {
                size_t read = 0;
                size_t left_unread =
                    did_first_read ? input_buffer_size - input_processed_bytes : 0;
                memcpy(input_buffer, input_buffer + input_processed_bytes, left_unread);

                did_first_read = true;
                input_processed_bytes = 0;

                read =
                    fread(input_buffer + left_unread, 1, input_buffer_size - left_unread, fp);
                if (read == 0) {
                    bytes_left = 0;
                    break;
                }
                if (read > bytes_left) {
                    bytes_left = 0;
                } else {
                    bytes_left -= (unsigned long)read;
                }
            }

            sample_count = mp3dec_decode_frame(
                t->mp3d, (unsigned char*)(input_buffer + input_processed_bytes),
                (int)(input_buffer_size - input_processed_bytes), pcm, &frame_info);
            if (sample_count == 0 && frame_info.frame_bytes == 0) {
                /* should never get here */
                bytes_left = 0;
                break;
            }

            if (sample_count > 0) {
                if (frame_info.channels != TE_SOUND_CHANNEL_COUNT
                    || frame_info.hz != TE_SOUND_HZ
                    || sample_count % TE_SOUND_CHANNEL_COUNT != 0) {
#if defined(DEBUG)
                    log_error_fmt(
                        __FILE__, __LINE__,
                        "expected %i channels, %i hz audio, got: channels: %i, hz: %i, "
                        "sample count: %i",
                        TE_SOUND_CHANNEL_COUNT, TE_SOUND_HZ, frame_info.channels,
                        frame_info.hz, sample_count);
#endif
                    break;
                }

                /* pass samples to the mixer */
                while (!t->exit_thread && !audio_mixer_is_ready_for_new_samples(mixer, t)) {
                    yield_thread();
                }
                if (t->exit_thread) {
                    break;
                }
                audio_mixer_add_new_samples(
                    mixer, t, pcm, (unsigned int)(sample_count * frame_info.channels));
            }

            input_processed_bytes += (size_t)frame_info.frame_bytes;
        };

        if (bytes_left == 0 && !t->exit_thread && !t->restart && !t->sound->loop) {
            /* play all left samples */
            audio_mixer_set_audio_source_wont_have_new_samples(mixer, t, true);

            /* wait for the rest to be sent to audio system */
            while (!t->exit_thread && !t->restart && !t->sound->loop
                   && audio_mixer_is_audio_source_have_samples_not_sent_yet(mixer, t)) {
                yield_thread();
            }

            {
                /* wait a bit for the left samples to be played */
                float buffer_ms =
                    ((float)(TE_SOUND_SAMPLE_COUNT_IN_BUFFER / TE_SOUND_CHANNEL_COUNT)
                     / (float)TE_SOUND_HZ)
                    * 1000.0f;
                sleep_ms((unsigned int)(TE_SOUND_BUFFER_COUNT * buffer_ms));
            }
        }
    } while ((t->sound->loop || t->restart) && !t->exit_thread);

audio_decoding_thread_cleanup:
    if (fp != NULL) {
        fclose(fp);
    }
    free(input_buffer);
    free(pcm);

    {
        te_sound* sound = t->sound;

        if (!t->exit_thread) {
            mutex_lock(sound->sm->mtx_audio_threads);
        }

        /* clear sound pointer in the thread to avoid tick updates */
        t->sound = NULL;

        sound->is_finished_playing = true;

        if (!t->exit_thread) {
            mutex_unlock(sound->sm->mtx_audio_threads);

            if (sound->on_finished != NULL) {
                sound->on_finished(sound->user_data, sound);
            }
        }
    }
}

static void
update_sound_3d_props(te_sound* sound) {
    te_vec3 to_sound;
    float len;

    te_sound_manager* sm = sound->sm;

    vec3_sub(sound->pos, sm->listener_pos, to_sound);

    /* attenuation */
    len = vec3_len(to_sound);
    if (len > sound->attenuation_distance) {
        sound->volume_attenuation = 0.0f;
    } else {
        sound->volume_attenuation =
            (sound->attenuation_distance - len) / sound->attenuation_distance;
    }

    /* pan */
    vec3_normalize(to_sound);
    sound->pan = vec3_dot(sm->listener_right, to_sound);
}

static void
init_audio_decoding_thread(
    te_audio_thread* t, te_audio_mixer* mixer, te_sound* sound,
    const char* relative_path_to_audio) {
    t->sound = sound;
    t->path_to_audio = filesystem_prepend_res_to_path(relative_path_to_audio, NULL);
    t->exit_thread = false;
    t->is_paused = false;
    t->restart = false;

    if (!filesystem_does_path_exists(t->path_to_audio)) {
        t->mp3d = NULL;
        t->thread = NULL;
        log_error_fmt(__FILE__, __LINE__, "path to audio %s does not exist", t->path_to_audio);
    } else {
        audio_mixer_register_audio_source(mixer, t);

        if (sound->spatial) {
            update_sound_3d_props(sound);
        }
        audio_mixer_set_audio_source_volume(
            mixer, t, sound->volume * sound->volume_attenuation);
        audio_mixer_set_audio_source_pan(mixer, t, sound->pan);

        t->mp3d = malloc(sizeof(mp3dec_t));
        mp3dec_init(t->mp3d);
        t->thread = thread_create(audio_decoding_thread, t);
    }
}

static void
deinit_audio_decoding_thread(te_sound* sound) {
    te_audio_thread* t = sound->audio_thread;

    t->exit_thread = true;
    if (t->thread != NULL) {
        thread_destroy(t->thread);
        audio_mixer_unregister_audio_source(sound->sm->mixer, t);
    }
    free(t->mp3d);
    free(t->path_to_audio);
}

static te_audio_thread*
create_audio_thread_locked(te_sound* sound) {
    te_audio_thread* t;
    te_sound_manager* sm = sound->sm;

    if (sm->audio_thread_count == sm->audio_thread_capacity) {
        te_audio_thread** threads;

        sm->audio_thread_capacity += 4;

        threads = malloc(sizeof(te_audio_thread*) * sm->audio_thread_capacity);
        memcpy(threads, sm->audio_threads, sizeof(te_audio_thread*) * sm->audio_thread_count);

        free(sm->audio_threads);
        sm->audio_threads = threads;
    }

    sm->audio_threads[sm->audio_thread_count] = malloc(sizeof(te_audio_thread));
    t = sm->audio_threads[sm->audio_thread_count];
    init_audio_decoding_thread(t, sm->mixer, sound, sound->relative_path_to_audio);
    sm->audio_thread_count += 1;

    return t;
}

static void
destroy_audio_thread_locked(te_sound* sound) {
    te_sound_manager* sm = sound->sm;

    deinit_audio_decoding_thread(sound);

    if (sm->audio_thread_count == 1) {
        sm->audio_thread_count = 0;
        sm->audio_thread_capacity = 0;
        free(sm->audio_threads[0]);
        free(sm->audio_threads);
        sm->audio_threads = NULL;
    } else {
        unsigned int i;
        for (i = 0; i < sm->audio_thread_count; i++) {
            if (sm->audio_threads[i] != sound->audio_thread) {
                continue;
            }

            free(sm->audio_threads[i]);

            if (i != sm->audio_thread_count - 1) {
                sm->audio_threads[i] = sm->audio_threads[sm->audio_thread_count - 1];
            }

            sm->audio_thread_count -= 1;
            return;
        }

        log_error(__FILE__, __LINE__, "unable to find the specified audio thread to destroy");
    }
}

te_sound*
sound_create(te_sound_manager* sound_manager, const char* relative_path) {
    te_sound* sound = malloc(sizeof(te_sound));

    sound->on_finished = NULL;
    sound->user_data = NULL;
    sound->audio_thread = NULL;
    sound->relative_path_to_audio = relative_path;
    sound->sm = sound_manager;
    sound->volume = 1.0f;
    sound->volume_attenuation = 1.0f;
    sound->attenuation_distance = 30.0f;
    sound->pan = 0.0f;
    sound->is_finished_playing = false;
    sound->loop = false;
    sound->spatial = false;
    vec3_zero(sound->pos);

    return sound;
}

void
sound_destroy(te_sound* sound) {
    te_sound_manager* sm = sound->sm;

    mutex_lock(sm->mtx_audio_threads);

    if (sound->audio_thread != NULL) {
        destroy_audio_thread_locked(sound);
        sound->audio_thread = NULL;
    }

    free(sound);

    mutex_unlock(sm->mtx_audio_threads);
}

void
sound_play(te_sound* sound, bool restart) {
    mutex_lock(sound->sm->mtx_audio_threads);

    sound->is_finished_playing = false;

    if (sound->audio_thread == NULL) {
        /* first time playing */
        sound->audio_thread = create_audio_thread_locked(sound);
    } else {
        if (restart) {
            sound->audio_thread->restart = true;
        } else {
            /* playing after being paused (not stopped) */
            sound->audio_thread->is_paused = false;
        }
    }

    mutex_unlock(sound->sm->mtx_audio_threads);
}

void
sound_pause(te_sound* sound) {
    mutex_lock(sound->sm->mtx_audio_threads);
    if (sound->audio_thread != NULL) {
        sound->audio_thread->is_paused = true;
    }
    mutex_unlock(sound->sm->mtx_audio_threads);
}

void
sound_stop(te_sound* sound) {
    mutex_lock(sound->sm->mtx_audio_threads);

    if (sound->audio_thread != NULL) {
        destroy_audio_thread_locked(sound);
        sound->audio_thread = NULL;
    }

    mutex_unlock(sound->sm->mtx_audio_threads);
}

void
sound_manager_set_volume(te_sound_manager* sound_manager, float volume) {
    audio_mixer_set_volume(sound_manager->mixer, volume);
}

bool
sound_is_playing(te_sound* sound) {
    return sound->audio_thread != NULL;
}

void
sound_set_volume(te_sound* sound, float volume) {
    sound->volume = volume;

    mutex_lock(sound->sm->mtx_audio_threads);
    if (sound->audio_thread != NULL) {
        audio_mixer_set_audio_source_volume(
            sound->sm->mixer, sound->audio_thread, sound->volume * sound->volume_attenuation);
    }
    mutex_unlock(sound->sm->mtx_audio_threads);
}

void
sound_set_is_looping(te_sound* sound, bool is_looping) {
    sound->loop = is_looping;
}

void
sound_set_pan(te_sound* sound, float pan) {
    sound->pan = pan;

    mutex_lock(sound->sm->mtx_audio_threads);
    if (sound->audio_thread != NULL) {
        audio_mixer_set_audio_source_pan(sound->sm->mixer, sound->audio_thread, pan);
    }
    mutex_unlock(sound->sm->mtx_audio_threads);
}

bool
sound_is_finished_playing(te_sound* sound) {
    return sound->is_finished_playing;
}

void
sound_set_3d_position(te_sound* sound, te_vec3 pos) {
    sound->spatial = true;
    vec3_copy(pos, sound->pos);
}

void
sound_set_distance(te_sound* sound, float distance) {
    sound->spatial = true;

    if (distance < 0.01f) {
        distance = 0.01f;
    }
    sound->attenuation_distance = distance;
}

void
prv_sound_manager_tick(
    te_sound_manager* sound_manager, te_vec3 listener_pos, te_vec3 listener_forward,
    te_vec3 listener_up) {
    unsigned int i;
#if defined(ENGINE_DEBUG_TOOLS)
    prv_debug_console_get_stats()->playing_sound_count = 0;
#endif

    vec3_copy(listener_pos, sound_manager->listener_pos);
    vec3_cross(listener_forward, listener_up, sound_manager->listener_right);

    mutex_lock(sound_manager->mtx_audio_threads);

    for (i = 0; i < sound_manager->audio_thread_count; i++) {
        te_audio_thread* t = sound_manager->audio_threads[i];

        if (t->sound == NULL) {
            continue;
        }

#if defined(ENGINE_DEBUG_TOOLS)
        prv_debug_console_get_stats()->playing_sound_count += 1;
#endif

        if (t->sound->spatial) {
            update_sound_3d_props(t->sound);
            audio_mixer_set_audio_source_volume(
                sound_manager->mixer, t, t->sound->volume * t->sound->volume_attenuation);
            audio_mixer_set_audio_source_pan(sound_manager->mixer, t, t->sound->pan);
        }
    }

    mutex_unlock(sound_manager->mtx_audio_threads);
}

void
prv_sound_set_on_finished_callback_audio_thread(
    te_sound* sound, void* user_data, void (*on_finished)(void* user_data, te_sound* sound)) {
    if (sound->on_finished != NULL) {
        log_error(__FILE__, __LINE__, "sound on_finished callback is already set");
        abort();
    }
    sound->user_data = user_data;
    sound->on_finished = on_finished;
}
