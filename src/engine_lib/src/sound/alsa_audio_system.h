#pragma once

#include <stdbool.h>
#include <sound/sound_constants.h>

typedef struct te_alsa_audio_system te_alsa_audio_system;

te_alsa_audio_system* alsa_audio_system_create(
    void (*on_need_next_audio_buffer)(void* userdata, unsigned char use_buffer_idx),
    void* userdata, short* buffers[TE_SOUND_BUFFER_COUNT], unsigned int buffer_size_bytes);
void alsa_audio_system_destroy(te_alsa_audio_system* sys);

void alsa_audio_system_on_audio_buffer_ready(te_alsa_audio_system* sys);

bool alsa_audio_system_is_playing(te_alsa_audio_system* sys);

unsigned char alsa_audio_system_get_next_buffer_idx(te_alsa_audio_system* sys);
