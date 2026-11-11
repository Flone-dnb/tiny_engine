#pragma once

#define TE_SOUND_CHANNEL_COUNT 2
#define TE_SOUND_HZ 44100

/* at least 2 is needed but 2 will often result in choppy sound */
#define TE_SOUND_BUFFER_COUNT 4

/* at 44100 hz 1024 samples (per channel) is about ~23 ms of latency
 * 512 results in choppy sound on old low end systems */
#define TE_SOUND_SAMPLE_COUNT_IN_BUFFER 1024 * TE_SOUND_CHANNEL_COUNT
