#include <window_system/os_window.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#if defined(WIN32)
#include <window_system/win32_window.h>
#endif

struct te_os_window {
    // platform-specific implementation data
    void* impl;

    // always valid
    te_os_window_callbacks* callbacks;

    void* user_data;

    // current size in pixels
    unsigned int width;
    unsigned int height;

    bool should_close;
};

te_os_window*
os_window_create(const char* title, te_os_window_callbacks* callbacks, void* user_data) {
    te_os_window* os_window = malloc(sizeof(te_os_window));
    memset(os_window, 0, sizeof(te_os_window));

    os_window->callbacks = malloc(sizeof(te_os_window_callbacks));
    memcpy(os_window->callbacks, callbacks, sizeof(te_os_window_callbacks));

    os_window->user_data = user_data;
    os_window->should_close = false;

#if defined(WIN32)
    win32_window_create(os_window, title);
#else
    // TODO: not implemented yet
    assert(false);
    abort();
#endif

    return os_window;
}

void
os_window_destroy(te_os_window* os_window) {
#if defined(WIN32)
    win32_window_destroy(os_window);
#else
    // TODO: not implemented yet
    assert(false);
    abort();
#endif

    free(os_window->callbacks);

    free(os_window);
}

void*
os_window_get_user_data(te_os_window* os_window) {
    return os_window->user_data;
}

void
os_window_close(te_os_window* os_window) {
    os_window->should_close = true;
}

bool
os_window_should_close(te_os_window* os_window) {
    return os_window->should_close;
}

void
prv_os_window_set_impl(te_os_window* os_window, void* impl) {
    os_window->impl = impl;
}

void*
prv_os_window_get_impl(te_os_window* os_window) {
    return os_window->impl;
}

void
os_window_capture_mouse_cursor(te_os_window* os_window, bool capture) {
#if defined(WIN32)
    win32_window_capture_mouse_cursor(os_window, capture);
#else
    // TODO: not implemented yet
    assert(false);
    abort();
#endif
}

void
os_window_poll_event(te_os_window* os_window) {
#if defined(WIN32)
    win32_window_poll_event(os_window);
#else
    // TODO: not implemented yet
    assert(false);
    abort();
#endif
}

void
os_window_swap_buffers(te_os_window* os_window) {
#if defined(WIN32)
    win32_window_swap_buffers(os_window);
#else
    // TODO: not implemented yet
    assert(false);
    abort();
#endif
}

void
os_window_set_cursor_position(te_os_window* os_window, float x, float y) {
#if defined(WIN32)
    win32_window_set_cursor_position(os_window, x, y);
#else
    // TODO: not implemented yet
    assert(false);
    abort();
#endif
}

void
os_window_get_cursor_position(te_os_window* os_window, float* x, float* y) {
#if defined(WIN32)
    win32_window_get_cursor_position(os_window, x, y);
#else
    // TODO: not implemented yet
    assert(false);
    abort();
#endif
}

void
os_window_get_size(te_os_window* os_window, unsigned int* width, unsigned int* height) {
    (*width) = os_window->width;
    (*height) = os_window->height;
}

unsigned int
os_window_get_refresh_rate(te_os_window* os_window) {
#if defined(WIN32)
    return win32_window_get_refresh_rate(os_window);
#else
    // TODO: not implemented yet
    assert(false);
    abort();
#endif
}

void
prv_os_window_on_size_changed(
    te_os_window* os_window, unsigned int width, unsigned int height) {
    os_window->width = width;
    os_window->height = height;
}

te_os_window_callbacks*
prv_os_window_get_callbacks(te_os_window* os_window) {
    return os_window->callbacks;
}