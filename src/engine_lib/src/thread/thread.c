#include <thread/thread.h>

#if defined(WIN32)
#include <thread/win32_thread.h>
#else
#include <thread/posix_thread.h>
#endif

te_thread*
thread_create(void (*run)(void* userdata), void* userdata) {
#if defined(WIN32)
    return win32_thread_create(run, userdata);
#else
    return posix_thread_create(run, userdata);
#endif
}

void
thread_destroy(te_thread* thread) {
#if defined(WIN32)
    win32_thread_destroy(thread);
#else
    posix_thread_destroy(thread);
#endif
}
