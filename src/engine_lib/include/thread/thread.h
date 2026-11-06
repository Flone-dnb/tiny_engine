#pragma once

typedef struct te_thread te_thread;

/* creates and immediately runs the thread */
te_thread* thread_create(void (*run)(void* userdata), void* userdata);

/* stops and destroys the thread */
void thread_destroy(te_thread* thread);
