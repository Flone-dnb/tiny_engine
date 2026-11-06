#include <thread/win32_thread.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <stdlib.h>
#include <process.h>
#include <io/log.h>

struct te_thread {
    void (*run)(void* userdata);
    void* userdata;
    HANDLE h;
};

static unsigned int WINAPI
thread_run(void* ptr) {
    te_thread* thread = ptr;
    thread->run(thread->userdata);
    return 0;
}

te_thread*
win32_thread_create(void (*run)(void* userdata), void* userdata) {
    te_thread* thread = malloc(sizeof(te_thread));

    thread->run = run;
    thread->userdata = userdata;

    thread->h = (HANDLE)_beginthreadex(NULL, 0, thread_run, thread, 0, NULL);
    if (thread->h == NULL) {
        log_error(__FILE__, __LINE__, "failed to create a new thread");
        abort();
    }

    return thread;
}

void
win32_thread_destroy(te_thread* thread) {
    WaitForSingleObject(thread->h, INFINITE);
    CloseHandle(thread->h);

    free(thread);
}
