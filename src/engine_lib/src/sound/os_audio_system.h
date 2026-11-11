#pragma once

#include <stdbool.h>
#include <sound/sound_constants.h>

typedef struct te_os_audio_system te_os_audio_system;

/* creates platform specific audio system */
te_os_audio_system* os_audio_system_create(
    void (*on_need_next_audio_buffer)(void* userdata, unsigned char use_buffer_idx),
    void* userdata, short* buffers[TE_SOUND_BUFFER_COUNT], unsigned int buffer_size_bytes);
void os_audio_system_destroy(te_os_audio_system* sys);

void os_audio_system_on_audio_buffer_ready(te_os_audio_system* sys);

bool os_audio_system_is_playing(te_os_audio_system* sys);

unsigned char os_audio_system_get_next_buffer_idx(te_os_audio_system* sys);
