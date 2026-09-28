#pragma once

#include <stdint.h>

uint64_t high_freq_timer_now(void);
float high_freq_timer_get_elapsed_ms(uint64_t start);
float high_freq_timer_get_elapsed_ms_range(uint64_t start, uint64_t end);