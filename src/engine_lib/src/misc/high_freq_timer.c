#include <misc/high_freq_timer.h>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#endif

te_hft_t
high_freq_timer_now(void) {
#if defined(_WIN32)
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return c.QuadPart;
#else
    struct timespec ts;
#if defined(CLOCK_MONOTONIC_RAW)
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
#endif
}

static te_hft_t
high_freq_timer_frequency(void) {
#if defined(_WIN32)
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f.QuadPart;
#else
    return 1000000000L;
#endif
}

float
high_freq_timer_get_elapsed_ms(te_hft_t start) {
    return (float)((double)(high_freq_timer_now() - start) * 1000.0
                   / (double)high_freq_timer_frequency());
}

float
high_freq_timer_get_elapsed_ms_range(te_hft_t start, te_hft_t end) {
    return (float)((double)(end - start) * 1000.0 / (double)high_freq_timer_frequency());
}
