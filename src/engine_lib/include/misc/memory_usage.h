#pragma once

#include <stddef.h>

// Returns the current resident set size (physical memory use) that this process is using.
// Size in bytes.
size_t memory_usage_get_process_used_memory(void);

// Returns the total physical memory (RAM) size.
// Size in bytes.
size_t memory_usage_get_total_memory(void);

// Returns the total physical memory (RAM) size that's being used.
// Size in bytes.
size_t memory_usage_get_total_used_memory(void);
