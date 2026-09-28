#include <misc/high_freq_timer.h>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <time.h>
#endif

uint64_t
high_freq_timer_now(void) {
#if defined(_WIN32)
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (uint64_t)c.QuadPart;
#else
    struct timespec ts;
#if defined(CLOCK_MONOTONIC_RAW)
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

static uint64_t
high_freq_timer_frequency(void) {
#if defined(_WIN32)
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return (uint64_t)f.QuadPart;
#else
    return 1000000000ULL;
#endif
}

float
high_freq_timer_get_elapsed_ms(uint64_t start) {
    return (float)((double)(high_freq_timer_now() - start) * 1000.0
                   / (double)high_freq_timer_frequency());
}

float
high_freq_timer_get_elapsed_ms_range(uint64_t start, uint64_t end) {
    return (float)((double)(end - start) * 1000.0
                   / (double)high_freq_timer_frequency());
}