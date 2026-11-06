#pragma once

#include <thread/mutex.h>

te_mutex* posix_mutex_create(void);
void posix_mutex_destroy(te_mutex* mutex);

void posix_mutex_lock(te_mutex* mutex);
void posix_mutex_unlock(te_mutex* mutex);
