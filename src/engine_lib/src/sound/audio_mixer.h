#pragma once

#include <stdbool.h>

typedef struct te_audio_mixer te_audio_mixer;

te_audio_mixer* audio_mixer_create(void);
void audio_mixer_destroy(te_audio_mixer* mixer);

/* specify value in range [0.0; 1.0] */
void audio_mixer_set_volume(te_audio_mixer* mixer, float volume);

/* the specified ID is some unique pointer */
void audio_mixer_register_audio_source(te_audio_mixer* mixer, void* id);
void audio_mixer_unregister_audio_source(te_audio_mixer* mixer, void* id);

/* specify value in range [0.0; 1.0] */
void audio_mixer_set_audio_source_volume(
    te_audio_mixer* mixer, void* audio_source_id, float volume);

/* specify value in range [-1.0f; 1.0] */
void audio_mixer_set_audio_source_pan(te_audio_mixer* mixer, void* audio_source_id, float pan);

/* specify `true` for the mixer to play the rest of the samples left */
void audio_mixer_set_audio_source_wont_have_new_samples(
    te_audio_mixer* mixer, void* audio_source_id, bool wont_have_new_samples);

/* returns `true` if there are some samples that were not sent to the audio system yet */
bool audio_mixer_is_audio_source_have_samples_not_sent_yet(
    te_audio_mixer* mixer, void* audio_source_id);

/* called by registered audio sources when a new bunch of audio samples are ready to be played
 * the specified sample_count should be for all channels (i.e. short count in the specified buffer)
 * BEFORE calling this function check @ref audio_mixer_is_ready_for_new_samples */
void audio_mixer_add_new_samples(
    te_audio_mixer* mixer, void* audio_source_id, short* samples, unsigned int sample_count);

/* tells if the specified audio source can call @ref audio_mixer_add_new_samples again */
bool audio_mixer_is_ready_for_new_samples(te_audio_mixer* mixer, void* audio_source_id);
