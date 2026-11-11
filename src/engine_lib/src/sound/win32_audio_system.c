#include <sound/win32_audio_system.h>

#include <stdlib.h>
#include <stdbool.h>
#include <io/log.h>
#include <thread/thread.h>
#include <thread/mutex.h>
#define WIN32_MEAN_AND_LEAN
#define NOMINMAX
#include <Windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

struct te_win32_audio_system {
    te_thread* thread;
    te_mutex* mtx_data_ready;

    HWAVEOUT wave_out;
    HANDLE state_change_event;

    WAVEHDR headers[TE_SOUND_BUFFER_COUNT];

    void (*on_need_next_audio_buffer)(void* userdata, unsigned char use_buffer_idx);
    void* userdata;

    unsigned char buffer_ready_count;
    unsigned char next_buffer_idx;

    bool is_being_destroyed;
};

static void audio_thread(te_win32_audio_system* sys);

static void
prepare_header(
    te_win32_audio_system* sys, WAVEHDR* hdr, short* buffer, unsigned int buffer_size_bytes,
    MMRESULT* result) {
    if ((*result) != MMSYSERR_NOERROR) {
        return;
    }
    hdr->lpData = (LPSTR)buffer;
    hdr->dwBufferLength = buffer_size_bytes;
    hdr->dwFlags = 0;
    (*result) = waveOutPrepareHeader(sys->wave_out, hdr, sizeof(WAVEHDR));
}

te_win32_audio_system*
win32_audio_system_create(
    void (*on_need_next_audio_buffer)(void* userdata, unsigned char use_buffer_idx),
    void* userdata, short* buffers[TE_SOUND_BUFFER_COUNT],
    unsigned int buffer_size_bytes) {
    unsigned int i;
    WAVEFORMATEX wfx = {0};
    MMRESULT result = MMSYSERR_NOERROR;

    te_win32_audio_system* sys = malloc(sizeof(te_win32_audio_system));

    sys->on_need_next_audio_buffer = on_need_next_audio_buffer;
    sys->userdata = userdata;

    sys->buffer_ready_count = 0;
    sys->is_being_destroyed = false;
    sys->next_buffer_idx = 0;
    sys->mtx_data_ready = mutex_create();

    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = TE_SOUND_CHANNEL_COUNT;
    wfx.nSamplesPerSec = TE_SOUND_HZ;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    sys->state_change_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    result = waveOutOpen(
        &sys->wave_out, WAVE_MAPPER, &wfx, (DWORD_PTR)sys->state_change_event, 0,
        CALLBACK_EVENT);
    if (result != MMSYSERR_NOERROR) {
        log_error_fmt(__FILE__, __LINE__, "failed to open audio device, error: %u", result);
        CloseHandle(sys->state_change_event);
        free(sys);
        return NULL;
    }

    for (i = 0; i < TE_SOUND_BUFFER_COUNT; i++) {
        prepare_header(sys, &sys->headers[i], buffers[i], buffer_size_bytes, &result);

        /* for our audio thread checks */
        sys->headers[i].dwFlags |= WHDR_DONE;
    }
    if (result != MMSYSERR_NOERROR) {
        log_error_fmt(__FILE__, __LINE__, "failed to prepare audio header, error: %u", result);
        CloseHandle(sys->state_change_event);
        free(sys);
        return NULL;
    }

    ResetEvent(sys->state_change_event);

    sys->thread = thread_create(audio_thread, sys);

    return sys;
}

void
win32_audio_system_destroy(te_win32_audio_system* sys) {
    unsigned int i;

    sys->is_being_destroyed = true;
    thread_destroy(sys->thread);
    mutex_destroy(sys->mtx_data_ready);

    waveOutReset(sys->wave_out);

    for (i = 0; i < TE_SOUND_BUFFER_COUNT; i++) {
        while (!(sys->headers[i].dwFlags & WHDR_DONE)) {
            Sleep(0);
        }
    }

    for (i = 0; i < TE_SOUND_BUFFER_COUNT; i++) {
        waveOutUnprepareHeader(sys->wave_out, &sys->headers[i], sizeof(WAVEHDR));
    }

    waveOutClose(sys->wave_out);

    CloseHandle(sys->state_change_event);

    free(sys);
}

void
win32_audio_system_on_audio_buffer_ready(te_win32_audio_system* sys) {
    mutex_lock(sys->mtx_data_ready);
    sys->buffer_ready_count += 1;
#if defined(DEBUG)
    if (sys->buffer_ready_count == TE_SOUND_BUFFER_COUNT + 1) {
        log_error(__FILE__, __LINE__, "too many audio buffers queued");
        abort();
    }
#endif
    mutex_unlock(sys->mtx_data_ready);
}

bool
win32_audio_system_is_playing(te_win32_audio_system* sys) {
    unsigned int i;
    bool is_done = true;

    mutex_lock(sys->mtx_data_ready);

    for (i = 0; i < TE_SOUND_BUFFER_COUNT; i++) {
        is_done &= (bool)(sys->headers[i].dwFlags & WHDR_DONE);
    }
    is_done &= sys->buffer_ready_count == 0;

    mutex_unlock(sys->mtx_data_ready);

    return !is_done;
}

unsigned char
win32_audio_system_get_next_buffer_idx(te_win32_audio_system* sys) {
    return sys->next_buffer_idx;
}

static void
audio_thread(te_win32_audio_system* sys) {
    unsigned int i;
    bool no_sound = true;
    MMRESULT result = MMSYSERR_NOERROR;
    DWORD state;

    while (!sys->is_being_destroyed) {
        if (sys->buffer_ready_count == 0) {
            no_sound = true;
            for (i = 0; i < TE_SOUND_BUFFER_COUNT; i++) {
                no_sound &= (bool)(sys->headers[i].dwFlags & WHDR_DONE);
            }
            if (no_sound) {
                Sleep(5);
            } else {
                Sleep(0);
            }
            continue;
        }

        mutex_lock(sys->mtx_data_ready);

        do {
            for (i = sys->next_buffer_idx;
                 i < TE_SOUND_BUFFER_COUNT && sys->buffer_ready_count > 0; i++) {
                if (!(sys->headers[i].dwFlags & WHDR_DONE)) {
                    log_error(__FILE__, __LINE__, "expected the specified audio buffer to be ready");
                    mutex_unlock(sys->mtx_data_ready);
                    return;
                }

                /* the driver will SetEvent after finished playing the buffer */
                result = waveOutWrite(sys->wave_out, &sys->headers[i], sizeof(WAVEHDR));
                if (result != MMSYSERR_NOERROR) {
                    break;
                }
                sys->buffer_ready_count -= 1;

                sys->next_buffer_idx =
                    (sys->next_buffer_idx + 1) % TE_SOUND_BUFFER_COUNT;
            }
        } while (sys->buffer_ready_count > 0);

        mutex_unlock(sys->mtx_data_ready);
        if (result != MMSYSERR_NOERROR) {
            break;
        }

        while (!(sys->headers[sys->next_buffer_idx].dwFlags & WHDR_DONE)) {
            do {
                /* supplying a timeout in case we need to destroy */
                /* if signaled auto-resets to not signaled here */
                state = WaitForSingleObject(sys->state_change_event, 50);
                if (state == WAIT_FAILED) {
                    log_error(__FILE__, __LINE__, "audio system failure");
                    return;
                }
            } while (state != WAIT_OBJECT_0 && !sys->is_being_destroyed);
        }

        if (!sys->is_being_destroyed) {
            sys->on_need_next_audio_buffer(sys->userdata, sys->next_buffer_idx);
        }
    }

    if (result != MMSYSERR_NOERROR) {
        char msg[MAXERRORLENGTH + 1] = {0};
        waveOutGetErrorText(result, msg, MAXERRORLENGTH);
        log_error_fmt(__FILE__, __LINE__, "audio system error: %s", msg);
    }
}
