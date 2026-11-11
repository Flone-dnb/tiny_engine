#include <sound/os_audio_system.h>

#if defined(WIN32)
#include <sound/win32_audio_system.h>
#else
#include <sound/alsa_audio_system.h>
#endif

te_os_audio_system*
os_audio_system_create(
    void (*on_need_next_audio_buffer)(void* userdata, unsigned char use_buffer_idx),
    void* userdata, short* buffers[TE_SOUND_BUFFER_COUNT], unsigned int buffer_size_bytes) {
#if defined(WIN32)
    return (te_os_audio_system*)win32_audio_system_create(
        on_need_next_audio_buffer, userdata, buffers, buffer_size_bytes);
#else
    return (te_os_audio_system*)alsa_audio_system_create(
        on_need_next_audio_buffer, userdata, buffers, buffer_size_bytes);
#endif
}

void
os_audio_system_destroy(te_os_audio_system* sys) {
#if defined(WIN32)
    win32_audio_system_destroy((te_win32_audio_system*)sys);
#else
    alsa_audio_system_destroy((te_alsa_audio_system*)sys);
#endif
}

void
os_audio_system_on_audio_buffer_ready(te_os_audio_system* sys) {
#if defined(WIN32)
    win32_audio_system_on_audio_buffer_ready((te_win32_audio_system*)sys);
#else
    alsa_audio_system_on_audio_buffer_ready((te_alsa_audio_system*)sys);
#endif
}

bool
os_audio_system_is_playing(te_os_audio_system* sys) {
#if defined(WIN32)
    return win32_audio_system_is_playing((te_win32_audio_system*)sys);
#else
    return alsa_audio_system_is_playing((te_alsa_audio_system*)sys);
#endif
}

unsigned char
os_audio_system_get_next_buffer_idx(te_os_audio_system* sys) {
#if defined(WIN32)
    return win32_audio_system_get_next_buffer_idx((te_win32_audio_system*)sys);
#else
    return alsa_audio_system_get_next_buffer_idx((te_alsa_audio_system*)sys);
#endif
}
