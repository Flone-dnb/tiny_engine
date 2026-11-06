#pragma once

typedef struct te_mutex te_mutex;

te_mutex* mutex_create(void);
void mutex_destroy(te_mutex* mutex);

void mutex_lock(te_mutex* mutex);
void mutex_unlock(te_mutex* mutex);
