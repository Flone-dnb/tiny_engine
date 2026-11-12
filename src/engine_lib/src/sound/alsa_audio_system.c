/* to compile ALSA in strict C89 we need this: */
#define _GNU_SOURCE
#define inline __inline__

#include <sound/alsa_audio_system.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <io/log.h>
#include <thread/thread.h>
#include <thread/mutex.h>

#include <alsa/asoundlib.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>

#define TE_ALSA_PERIOD_COUNT 4

struct te_alsa_audio_system {
    te_thread* thread;
    te_mutex* mtx_data_ready;

    snd_pcm_t* pcm;

    short* buffers[TE_SOUND_BUFFER_COUNT];
    snd_pcm_uframes_t period_frames;

    bool buffer_in_flight[TE_SOUND_BUFFER_COUNT];

    void (*on_need_next_audio_buffer)(void* userdata, unsigned char use_buffer_idx);
    void* userdata;

    unsigned char buffer_ready_count;
    unsigned char next_buffer_idx;

    bool is_being_destroyed;
};

static void audio_thread(te_alsa_audio_system* sys);

static void
sleep_ms(unsigned int ms) {
    struct timeval tv;

    if (ms == 0) {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
    } else {
        tv.tv_sec = ms / 1000u;
        tv.tv_usec = (ms % 1000u) * 1000u;
    }

    (void)select(0, NULL, NULL, NULL, &tv);
}

static int
alsa_open_device(te_alsa_audio_system* sys, unsigned int buffer_size_bytes) {
    int err;
    unsigned int rate = TE_SOUND_HZ;
    unsigned int channels = TE_SOUND_CHANNEL_COUNT;
    snd_pcm_hw_params_t* hw = NULL;
    snd_pcm_sw_params_t* sw = NULL;
    snd_pcm_uframes_t period_size;
    snd_pcm_uframes_t buffer_size;
    unsigned int period_count = TE_ALSA_PERIOD_COUNT;

    err = snd_pcm_open(&sys->pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        log_error_fmt(__FILE__, __LINE__, "failed to open ALSA device: %s", snd_strerror(err));
        return err;
    }

    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_sw_params_alloca(&sw);

    err = snd_pcm_hw_params_any(sys->pcm, hw);
    if (err < 0) {
        log_error_fmt(
            __FILE__, __LINE__, "no playback config available: %s", snd_strerror(err));
        goto fail;
    }

    err = snd_pcm_hw_params_set_access(sys->pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        log_error_fmt(__FILE__, __LINE__, "failed to set access: %s", snd_strerror(err));
        goto fail;
    }

    err = snd_pcm_hw_params_set_format(sys->pcm, hw, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        log_error_fmt(__FILE__, __LINE__, "failed to set format: %s", snd_strerror(err));
        goto fail;
    }

    err = snd_pcm_hw_params_set_channels(sys->pcm, hw, channels);
    if (err < 0) {
        log_error_fmt(__FILE__, __LINE__, "failed to set channels: %s", snd_strerror(err));
        goto fail;
    }

    err = snd_pcm_hw_params_set_rate_near(sys->pcm, hw, &rate, 0);
    if (err < 0) {
        log_error_fmt(__FILE__, __LINE__, "failed to set rate: %s", snd_strerror(err));
        goto fail;
    }

    /* ALSA period == one of our buffers */
    period_size = buffer_size_bytes / (TE_SOUND_CHANNEL_COUNT * (unsigned int)sizeof(short));

    err = snd_pcm_hw_params_set_period_size(sys->pcm, hw, period_size, 0);
    if (err < 0) {
        log_error_fmt(
            __FILE__, __LINE__, "failed to set period size %lu: %s",
            (unsigned long)period_size, snd_strerror(err));
        goto fail;
    }

    buffer_size = period_size * period_count;
    err = snd_pcm_hw_params_set_buffer_size_near(sys->pcm, hw, &buffer_size);
    if (err < 0) {
        log_error_fmt(__FILE__, __LINE__, "failed to set buffer size: %s", snd_strerror(err));
        goto fail;
    }

    err = snd_pcm_hw_params(sys->pcm, hw);
    if (err < 0) {
        log_error_fmt(__FILE__, __LINE__, "failed to apply hw params: %s", snd_strerror(err));
        goto fail;
    }

    /* re-read the period size actually granted (may differ slightly) */
    snd_pcm_hw_params_get_period_size(hw, &period_size, 0);
    sys->period_frames = period_size;

    err = snd_pcm_sw_params_current(sys->pcm, sw);
    if (err < 0) {
        log_error_fmt(__FILE__, __LINE__, "failed to get sw params: %s", snd_strerror(err));
        goto fail;
    }

    err = snd_pcm_sw_params_set_start_threshold(sys->pcm, sw, period_size);
    if (err < 0) {
        log_error_fmt(
            __FILE__, __LINE__, "failed to set start threshold: %s", snd_strerror(err));
        goto fail;
    }

    err = snd_pcm_sw_params_set_avail_min(sys->pcm, sw, period_size);
    if (err < 0) {
        log_error_fmt(__FILE__, __LINE__, "failed to set avail_min: %s", snd_strerror(err));
        goto fail;
    }

    err = snd_pcm_sw_params(sys->pcm, sw);
    if (err < 0) {
        log_error_fmt(__FILE__, __LINE__, "failed to apply sw params: %s", snd_strerror(err));
        goto fail;
    }

    snd_pcm_prepare(sys->pcm);
    return 0;

fail:
    snd_pcm_close(sys->pcm);
    sys->pcm = NULL;
    return err;
}

te_alsa_audio_system*
alsa_audio_system_create(
    void (*on_need_next_audio_buffer)(void* userdata, unsigned char use_buffer_idx),
    void* userdata, short* buffers[TE_SOUND_BUFFER_COUNT], unsigned int buffer_size_bytes) {
    unsigned int i;
    int err;

    te_alsa_audio_system* sys = malloc(sizeof(te_alsa_audio_system));

    sys->on_need_next_audio_buffer = on_need_next_audio_buffer;
    sys->userdata = userdata;

    sys->buffer_ready_count = 0;
    sys->is_being_destroyed = false;
    sys->next_buffer_idx = 0;
    sys->pcm = NULL;
    sys->mtx_data_ready = mutex_create();

    for (i = 0; i < TE_SOUND_BUFFER_COUNT; i++) {
        sys->buffers[i] = buffers[i];
        sys->buffer_in_flight[i] = false;
    }

    err = alsa_open_device(sys, buffer_size_bytes);
    if (err < 0) {
        mutex_destroy(sys->mtx_data_ready);
        free(sys);
        return NULL;
    }

    sys->thread = thread_create(audio_thread, sys);

    return sys;
}

void
alsa_audio_system_destroy(te_alsa_audio_system* sys) {
    sys->is_being_destroyed = true;
    thread_destroy(sys->thread);
    mutex_destroy(sys->mtx_data_ready);

    if (sys->pcm) {
        snd_pcm_drop(sys->pcm);
        snd_pcm_close(sys->pcm);
    }

    free(sys);
}

void
alsa_audio_system_on_audio_buffer_ready(te_alsa_audio_system* sys) {
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
alsa_audio_system_is_playing(te_alsa_audio_system* sys) {
    unsigned int i;
    bool is_done = true;

    mutex_lock(sys->mtx_data_ready);

    for (i = 0; i < TE_SOUND_BUFFER_COUNT; i++) {
        is_done &= !sys->buffer_in_flight[i];
    }
    is_done &= sys->buffer_ready_count == 0;

    mutex_unlock(sys->mtx_data_ready);

    return !is_done;
}

unsigned char
alsa_audio_system_get_next_buffer_idx(te_alsa_audio_system* sys) {
    return sys->next_buffer_idx;
}

static int
alsa_write_full_period(te_alsa_audio_system* sys, short* buffer) {
    snd_pcm_sframes_t written;
    snd_pcm_uframes_t frames = sys->period_frames;

    written = snd_pcm_writei(sys->pcm, buffer, frames);
    if (written == -EPIPE) {
        log_error(__FILE__, __LINE__, "ALSA underrun, recovering");
        if (snd_pcm_recover(sys->pcm, (int)written, 1) < 0) {
            return (int)written;
        }
        written = snd_pcm_writei(sys->pcm, buffer, frames);
    } else if (written == -ESTRPIPE) {
        int err;
        while ((err = snd_pcm_resume(sys->pcm)) == -EAGAIN) {
            sleep_ms(1);
        }
        if (err < 0) {
            if (snd_pcm_prepare(sys->pcm) < 0) {
                return err;
            }
        }
        written = snd_pcm_writei(sys->pcm, buffer, frames);
    }

    if (written < 0) {
        return (int)written;
    }
    if ((snd_pcm_uframes_t)written != frames) {
        log_error_fmt(
            __FILE__, __LINE__, "short ALSA write: %ld/%lu", (long)written,
            (unsigned long)frames);
        return -EIO;
    }
    return 0;
}

/* wait up to timeout_ms for the PCM to become writable
 * returns 0 on writable (or timeout — caller re-checks avail), <0 on hard error */
static int
alsa_wait_writable(te_alsa_audio_system* sys, int timeout_ms) {
    struct pollfd pfds[16];
    int nfds;
    int r;

    nfds = snd_pcm_poll_descriptors_count(sys->pcm);
    if (nfds <= 0 || nfds > (int)(sizeof(pfds) / sizeof(pfds[0]))) {
        sleep_ms((unsigned int)timeout_ms);
        return 0;
    }

    snd_pcm_poll_descriptors(sys->pcm, pfds, (unsigned int)nfds);

    r = poll(pfds, (nfds_t)nfds, timeout_ms);
    if (r < 0) {
        if (errno == EINTR) {
            return 0;
        }
        return -errno;
    }
    return 0;
}

static void
audio_thread(te_alsa_audio_system* sys) {
    int err = 0;

    while (!sys->is_being_destroyed) {
        unsigned char idx;
        unsigned char pending;

        mutex_lock(sys->mtx_data_ready);
        pending = sys->buffer_ready_count;
        idx = sys->next_buffer_idx;
        mutex_unlock(sys->mtx_data_ready);

        if (pending == 0) {
            bool any_in_flight = false;
            unsigned int i;

            mutex_lock(sys->mtx_data_ready);
            for (i = 0; i < TE_SOUND_BUFFER_COUNT; i++) {
                any_in_flight |= sys->buffer_in_flight[i];
            }
            mutex_unlock(sys->mtx_data_ready);

            sleep_ms(any_in_flight ? 0 : 5);
            continue;
        }

        if (sys->buffer_in_flight[idx]) {
            log_error(__FILE__, __LINE__, "expected the specified audio buffer to be ready");
            return;
        }

        /* wait until the driver has room for one more period */
        {
            snd_pcm_sframes_t avail;
            avail = snd_pcm_avail_update(sys->pcm);
            if (avail < 0) {
                err = snd_pcm_recover(sys->pcm, (int)avail, 1);
                if (err < 0) {
                    break;
                }
                continue;
            }
            if ((snd_pcm_uframes_t)avail < sys->period_frames) {
                err = alsa_wait_writable(sys, 50);
                if (err < 0) {
                    break;
                }
                continue;
            }
        }

        err = alsa_write_full_period(sys, sys->buffers[idx]);
        if (err < 0) {
            break;
        }

        /* this buffer is now owned by the driver, advance. */
        mutex_lock(sys->mtx_data_ready);
        sys->buffer_ready_count -= 1;
        sys->buffer_in_flight[idx] = true;
        sys->next_buffer_idx =
            (unsigned char)((sys->next_buffer_idx + 1) % TE_SOUND_BUFFER_COUNT);
        mutex_unlock(sys->mtx_data_ready);

        /* win32 loop fired on_need_next_audio_buffer after the driver signalled completion
	 * of the buffer at next_buffer_idx, in ALSA, "completion of that buffer" == "avail >= period_frames again"
         * wait for that, then run the callback */
        {
            snd_pcm_sframes_t avail;
            while (1) {
                avail = snd_pcm_avail_update(sys->pcm);
                if (avail < 0) {
                    err = snd_pcm_recover(sys->pcm, (int)avail, 1);
                    if (err < 0) {
                        goto out;
                    }
                    continue;
                }
                if ((snd_pcm_uframes_t)avail >= sys->period_frames) {
                    break;
                }
                if (sys->is_being_destroyed) {
                    goto out;
                }
                err = alsa_wait_writable(sys, 50);
                if (err < 0) {
                    goto out;
                }
            }
        }

        if (!sys->is_being_destroyed) {
            unsigned char completed = idx;
            mutex_lock(sys->mtx_data_ready);
            sys->buffer_in_flight[completed] = false;
            mutex_unlock(sys->mtx_data_ready);

            sys->on_need_next_audio_buffer(sys->userdata, sys->next_buffer_idx);
        }
    }

out:
    if (err < 0) {
        log_error_fmt(__FILE__, __LINE__, "audio system error: %s", snd_strerror(err));
    }
}
