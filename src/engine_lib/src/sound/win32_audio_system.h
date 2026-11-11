#pragma once

#include <stdbool.h>
#include <sound/sound_constants.h>

typedef struct te_win32_audio_system te_win32_audio_system;

te_win32_audio_system* win32_audio_system_create(
    void (*on_need_next_audio_buffer)(void* userdata, unsigned char use_buffer_idx), void* userdata,
    short* buffers[TE_SOUND_BUFFER_COUNT], unsigned int buffer_size_bytes);
void win32_audio_system_destroy(te_win32_audio_system* sys);

void win32_audio_system_on_audio_buffer_ready(te_win32_audio_system* sys);

bool win32_audio_system_is_playing(te_win32_audio_system* sys);

unsigned char win32_audio_system_get_next_buffer_idx(te_win32_audio_system* sys);
