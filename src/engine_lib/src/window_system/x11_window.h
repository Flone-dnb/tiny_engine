#pragma once

#if defined(__linux__)

#include <stdbool.h>

struct te_os_window;

void x11_window_create(struct te_os_window* os_window, const char* title);
void x11_window_destroy(struct te_os_window* os_window);

void x11_window_swap_buffers(struct te_os_window* os_window);

void x11_window_set_cursor_position(struct te_os_window* os_window, float x, float y);
void x11_window_get_cursor_position(struct te_os_window* os_window, float* x, float* y);

void x11_window_capture_mouse_cursor(struct te_os_window* os_window, bool capture);

unsigned int x11_window_get_refresh_rate(struct te_os_window* os_window);

bool x11_window_is_gamepad_connected(struct te_os_window* os_window);

void x11_window_poll_event(struct te_os_window* os_window);

#endif
