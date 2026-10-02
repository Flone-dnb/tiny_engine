#pragma once

#if defined(WIN32)
typedef __int64 te_hft_t;
#else
typedef long te_hft_t;
#endif

te_hft_t high_freq_timer_now(void);
float high_freq_timer_get_elapsed_ms(te_hft_t start);
float high_freq_timer_get_elapsed_ms_range(te_hft_t start, te_hft_t end);
