#pragma once

#if defined(WIN32)

#include <stdbool.h>

struct te_os_window;

void win32_window_create(struct te_os_window* os_window, const char* title);
void win32_window_destroy(struct te_os_window* os_window);

void win32_window_swap_buffers(struct te_os_window* os_window);

void win32_window_set_cursor_position(struct te_os_window* os_window, float x, float y);
void win32_window_get_cursor_position(struct te_os_window* os_window, float* x, float* y);

void win32_window_capture_mouse_cursor(struct te_os_window* os_window, bool capture);

unsigned int win32_window_get_refresh_rate(struct te_os_window* os_window);

bool win32_window_is_gamepad_connected(struct te_os_window* os_window);

void win32_window_poll_event(struct te_os_window* os_window);

#endif
