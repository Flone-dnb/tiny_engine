#include <thread/win32_mutex.h>

#if defined(WIN32)
#include <thread/win32_mutex.h>
#else
#include <thread/posix_mutex.h>
#endif

te_mutex* mutex_create(void) {
#if defined(WIN32)
    return win32_mutex_create();
#else
    return posix_mutex_create();
#endif
}

void mutex_destroy(te_mutex* mutex) {
#if defined(WIN32)
    win32_mutex_destroy(mutex);
#else
    posix_mutex_destroy(mutex);
#endif
}

void mutex_lock(te_mutex* mutex) {
#if defined(WIN32)
    win32_mutex_lock(mutex);
#else
    posix_mutex_lock(mutex);
#endif
}

void mutex_unlock(te_mutex* mutex) {
#if defined(WIN32)
    win32_mutex_unlock(mutex);
#else
    posix_mutex_unlock(mutex);
#endif
}
