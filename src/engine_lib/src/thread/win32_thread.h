#pragma once

#include <thread/thread.h>

/* creates and immediately runs the thread */
te_thread* win32_thread_create(void (*run)(void* userdata), void* userdata);

/* stops and destroys the thread */
void win32_thread_destroy(te_thread* thread);
