#include <thread/win32_mutex.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <stdlib.h>

struct te_mutex {
    CRITICAL_SECTION cs;
};

te_mutex* win32_mutex_create(void) {
    te_mutex* mutex = malloc(sizeof(te_mutex));
    InitializeCriticalSection(&mutex->cs);

    return mutex;
}

void win32_mutex_destroy(te_mutex* mutex) {
    DeleteCriticalSection(&mutex->cs);
    free(mutex);
}

void win32_mutex_lock(te_mutex* mutex) {
    EnterCriticalSection(&mutex->cs);
}

void win32_mutex_unlock(te_mutex* mutex) {
    LeaveCriticalSection(&mutex->cs);
}
