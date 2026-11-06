#include <thread/posix_thread.h>

#include <stdlib.h>
#include <pthread.h>
#include <io/log.h>

struct te_thread {
    void (*run)(void* userdata);
    void* userdata;
    pthread_t t;
};

static void* thread_run(void* ptr) {
    te_thread* thread = ptr;
    thread->run(thread->userdata);

    return NULL;
}

te_thread*
posix_thread_create(void (*run)(void* userdata), void* userdata) {
    te_thread* thread = malloc(sizeof(te_thread));

    thread->run = run;
    thread->userdata = userdata;

    if (pthread_create(&thread->t, NULL, thread_run, thread) != 0) {
        log_error(__FILE__, __LINE__, "failed to create a new thread");
        abort();
    }

    return thread;
}

void posix_thread_destroy(te_thread* thread) {
    pthread_join(thread->t, NULL);

    free(thread);
}
