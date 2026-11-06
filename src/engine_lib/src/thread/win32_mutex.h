#pragma once

#include <thread/mutex.h>

te_mutex* win32_mutex_create(void);
void win32_mutex_destroy(te_mutex* mutex);

void win32_mutex_lock(te_mutex* mutex);
void win32_mutex_unlock(te_mutex* mutex);
