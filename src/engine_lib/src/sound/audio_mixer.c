#include <sound/audio_mixer.h>

#include <stdlib.h>
#include <string.h>
#include <io/log.h>
#include <thread/mutex.h>
#include <limits.h>
#include <sound/os_audio_system.h>
#include <sound/sound_constants.h>

typedef struct te_audio_source {
    /* unique pointer of the audio source, do not free */
    void* id;

    /* must be freed on destruction */
    short* samples;

    unsigned int sample_count;
    unsigned int sample_capacity;

    /* in range [0.0; 1.0] */
    float volume;

    /* in range [-1.0f, 1.0f] */
    float pan;

    bool wont_have_new_samples;
} te_audio_source;

struct te_audio_mixer {
    /* double buffering for audio output */
    short* out_buffers[TE_SOUND_BUFFER_COUNT];
    te_mutex* mtx_out_buffer;

    te_os_audio_system* audio_system;

    te_audio_source* sources;
    unsigned int source_count;
    unsigned int source_capacity;

    /* in range [0.0; 1.0] */
    float volume;
};

static void on_need_next_audio_buffer(te_audio_mixer* mixer, unsigned char use_buffer_idx);

te_audio_mixer*
audio_mixer_create(void) {
    unsigned int i;

    te_audio_mixer* mixer = malloc(sizeof(te_audio_mixer));

    mixer->sources = NULL;
    mixer->source_count = 0;
    mixer->source_capacity = 0;
    mixer->volume = 1.0f;
    for (i = 0; i < TE_SOUND_BUFFER_COUNT; i++) {
        mixer->out_buffers[i] = malloc(sizeof(short) * TE_SOUND_SAMPLE_COUNT_IN_BUFFER);
    }
    mixer->mtx_out_buffer = mutex_create();

    mixer->audio_system = os_audio_system_create(
        on_need_next_audio_buffer, mixer, mixer->out_buffers,
        sizeof(short) * TE_SOUND_SAMPLE_COUNT_IN_BUFFER);

    return mixer;
}

static void
init_audio_source(te_audio_source* s, void* id) {
    s->id = id;
    s->sample_capacity = TE_SOUND_SAMPLE_COUNT_IN_BUFFER * (TE_SOUND_BUFFER_COUNT + 1);
    s->sample_count = 0;
    s->samples = malloc(sizeof(short) * s->sample_capacity);
    s->volume = 1.0f;
    s->pan = 0.0f;
    s->wont_have_new_samples = false;
}

static void
deinit_audio_source(te_audio_source* s) {
    free(s->samples);
}

void
audio_mixer_destroy(te_audio_mixer* mixer) {
    unsigned int i;

    os_audio_system_destroy(mixer->audio_system);

    for (i = 0; i < mixer->source_count; i++) {
        deinit_audio_source(&mixer->sources[i]);
    }
    free(mixer->sources);

    for (i = 0; i < TE_SOUND_BUFFER_COUNT; i++) {
        free(mixer->out_buffers[i]);
    }
    mutex_destroy(mixer->mtx_out_buffer);

    free(mixer);
}

static void
on_need_next_audio_buffer(te_audio_mixer* mixer, unsigned char use_buffer_idx) {
    short* samples;
    te_audio_source* s;
    unsigned int src_idx, j, sample_count, max_wrote_sample_count;
    int sample32;
    short sample;
    float samplef;
    bool wrote_samples = false;

    mutex_lock(mixer->mtx_out_buffer);

    samples = mixer->out_buffers[use_buffer_idx];

    max_wrote_sample_count = 0;

    /* mix different audio sources */
    for (src_idx = 0; src_idx < mixer->source_count; src_idx++) {
        s = &mixer->sources[src_idx];

        if (s->wont_have_new_samples) {
            if (s->sample_count == 0) {
                continue;
            }
            sample_count = s->sample_count;
            if (sample_count > TE_SOUND_SAMPLE_COUNT_IN_BUFFER) {
                sample_count = TE_SOUND_SAMPLE_COUNT_IN_BUFFER;
            }
        } else {
            if (s->sample_count < TE_SOUND_SAMPLE_COUNT_IN_BUFFER) {
                continue;
            }
            sample_count = TE_SOUND_SAMPLE_COUNT_IN_BUFFER;
        }

        if (sample_count > max_wrote_sample_count) {
            max_wrote_sample_count = sample_count;
        }

#define PROCESS_SAMPLE                                                                        \
    samplef = (float)s->samples[j] * s->volume;                                               \
    if (s->pan > 0.0f && j % 2 == 0) {                                                        \
        samplef *= 1.0f - s->pan;                                                             \
    } else if (s->pan < 0.0f && j % 2 != 0) {                                                 \
        samplef *= s->pan + 1.0f;                                                             \
    }                                                                                         \
    sample = (short)samplef;

        if (!wrote_samples) {
            wrote_samples = true;

            for (j = 0; j < sample_count; j++) {
                PROCESS_SAMPLE
                samples[j] = sample;
            }
        } else {
            /* mix with existing samples */
            for (j = 0; j < sample_count; j++) {
                PROCESS_SAMPLE
                sample32 = (int)samples[j] + (int)sample;

                /* clamp */
                if (sample32 > SHRT_MAX) {
                    sample32 = SHRT_MAX;
                } else if (sample32 < SHRT_MIN) {
                    sample32 = SHRT_MIN;
                }

                samples[j] = (short)sample32;
            }
        }

        /* TODO: come up with a better idea to "consume" samples */
        s->sample_count -= sample_count;
        memmove(s->samples, s->samples + sample_count, sizeof(short) * s->sample_count);
    }

    if (wrote_samples) {
        if (max_wrote_sample_count < TE_SOUND_SAMPLE_COUNT_IN_BUFFER) {
            /* add silence */
            memset(
                samples + max_wrote_sample_count, 0,
                sizeof(short) * (TE_SOUND_SAMPLE_COUNT_IN_BUFFER - max_wrote_sample_count));
        }

        if (mixer->volume != 1.0f) {
            /* apply master volume */
            for (j = 0; j < max_wrote_sample_count; j++) {
                samples[j] = (short)((float)samples[j] * mixer->volume);
            }
        }

        os_audio_system_on_audio_buffer_ready(mixer->audio_system);
    }

    mutex_unlock(mixer->mtx_out_buffer);
}

void
audio_mixer_register_audio_source(te_audio_mixer* mixer, void* id) {
    te_audio_source* s;

    mutex_lock(mixer->mtx_out_buffer);

    if (mixer->source_count == mixer->source_capacity) {
        te_audio_source* sources;

        mixer->source_capacity += 4;

        sources = malloc(sizeof(te_audio_source) * mixer->source_capacity);
        memcpy(sources, mixer->sources, sizeof(te_audio_source) * mixer->source_count);

        free(mixer->sources);
        mixer->sources = sources;
    }

    s = &mixer->sources[mixer->source_count];
    mixer->source_count += 1;

    init_audio_source(s, id);

    mutex_unlock(mixer->mtx_out_buffer);
}

void
audio_mixer_unregister_audio_source(te_audio_mixer* mixer, void* id) {
    mutex_lock(mixer->mtx_out_buffer);

    if (mixer->source_count == 1) {
        deinit_audio_source(&mixer->sources[0]);
        free(mixer->sources);
        mixer->sources = 0;
        mixer->source_count = 0;
        mixer->source_capacity = 0;
    } else {
        unsigned int i;
        for (i = 0; i < mixer->source_count; i++) {
            if (mixer->sources[i].id != id) {
                continue;
            }

            deinit_audio_source(&mixer->sources[i]);

            if (i != mixer->source_count - 1) {
                memcpy(
                    mixer->sources + i, mixer->sources + (mixer->source_count - 1),
                    sizeof(te_audio_source));
            }

            mixer->source_count -= 1;
            break;
        }
    }

    mutex_unlock(mixer->mtx_out_buffer);
}

bool
audio_mixer_is_audio_source_have_samples_not_sent_yet(
    te_audio_mixer* mixer, void* audio_source_id) {
    unsigned int i;
    for (i = 0; i < mixer->source_count; i++) {
        if (mixer->sources[i].id != audio_source_id) {
            continue;
        }

        return mixer->sources[i].sample_count != 0;
    }

    return false;
}

void
audio_mixer_set_audio_source_volume(
    te_audio_mixer* mixer, void* audio_source_id, float volume) {
    unsigned int i;
    for (i = 0; i < mixer->source_count; i++) {
        if (mixer->sources[i].id != audio_source_id) {
            continue;
        }

        mixer->sources[i].volume = volume;
        break;
    }
}

void
audio_mixer_set_audio_source_pan(te_audio_mixer* mixer, void* audio_source_id, float pan) {
    unsigned int i;
    for (i = 0; i < mixer->source_count; i++) {
        if (mixer->sources[i].id != audio_source_id) {
            continue;
        }

        mixer->sources[i].pan = pan;
        break;
    }
}

void
audio_mixer_set_audio_source_wont_have_new_samples(
    te_audio_mixer* mixer, void* audio_source_id, bool wont_have_new_samples) {
    unsigned int i;
    for (i = 0; i < mixer->source_count; i++) {
        if (mixer->sources[i].id != audio_source_id) {
            continue;
        }

        mixer->sources[i].wont_have_new_samples = wont_have_new_samples;
        break;
    }
}

void
audio_mixer_set_volume(te_audio_mixer* mixer, float volume) {
    mixer->volume = volume;
}

void
audio_mixer_add_new_samples(
    te_audio_mixer* mixer, void* audio_source_id, short* samples, unsigned int sample_count) {
    unsigned int i;
    unsigned int max_sample_len = 0;

    mutex_lock(mixer->mtx_out_buffer);

    for (i = 0; i < mixer->source_count; i++) {
        if (mixer->sources[i].id == audio_source_id) {
            te_audio_source* s = &mixer->sources[i];

            if (s->sample_count + sample_count > s->sample_capacity) {
                short* samples;

                s->sample_capacity =
                    s->sample_count + sample_count + TE_SOUND_SAMPLE_COUNT_IN_BUFFER;

                samples = malloc(sizeof(short) * s->sample_capacity);
                memcpy(samples, s->samples, sizeof(short) * s->sample_count);

                free(s->samples);
                s->samples = samples;
            }

            memcpy(s->samples + s->sample_count, samples, sizeof(short) * sample_count);
            s->sample_count += sample_count;

            /* note: don't break we need to calculate max sample len */
        }

        if (mixer->sources[i].sample_count > max_sample_len) {
            max_sample_len = mixer->sources[i].sample_count;
        }
    }

    mutex_unlock(mixer->mtx_out_buffer);

    /* see if we the audio system can be started by manually triggering the callback
     * (otherwise wait for the audio system to trigger the callback) */
    if (!os_audio_system_is_playing(mixer->audio_system)
        && max_sample_len >= TE_SOUND_SAMPLE_COUNT_IN_BUFFER * 2) {
        unsigned char use_buffer_idx =
            os_audio_system_get_next_buffer_idx(mixer->audio_system);
        on_need_next_audio_buffer(mixer, use_buffer_idx);
        on_need_next_audio_buffer(mixer, (use_buffer_idx + 1) % TE_SOUND_BUFFER_COUNT);
    }
}

bool
audio_mixer_is_ready_for_new_samples(te_audio_mixer* mixer, void* audio_source_id) {
    bool can_add_samples = false;
    unsigned int i;

    mutex_lock(mixer->mtx_out_buffer);

    for (i = 0; i < mixer->source_count; i++) {
        if (mixer->sources[i].id != audio_source_id) {
            continue;
        }

        can_add_samples = mixer->sources[i].sample_count
                          < TE_SOUND_SAMPLE_COUNT_IN_BUFFER * TE_SOUND_BUFFER_COUNT;
        break;
    }

    mutex_unlock(mixer->mtx_out_buffer);

    return can_add_samples;
}
