#include <thread/posix_mutex.h>

#include <stdlib.h>
#include <pthread.h>

struct te_mutex {
    pthread_mutex_t mtx;
};

te_mutex*
posix_mutex_create(void) {
    te_mutex* mutex = malloc(sizeof(te_mutex));
    pthread_mutex_init(&mutex->mtx, NULL);

    return mutex;
}

void
posix_mutex_destroy(te_mutex* mutex) {
    pthread_mutex_destroy(&mutex->mtx);

    free(mutex);
}

void
posix_mutex_lock(te_mutex* mutex) {
    pthread_mutex_lock(&mutex->mtx);
}

void
posix_mutex_unlock(te_mutex* mutex) {
    pthread_mutex_unlock(&mutex->mtx);
}
