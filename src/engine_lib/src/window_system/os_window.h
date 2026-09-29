#pragma once

#include <stdbool.h>
#include <input/mouse_button.h>
#include <input/keyboard_button.h>
#include <input/gamepad_button.h>

// os-specific window wrapper
typedef struct te_os_window te_os_window;

// configurable options
#if !defined(ENGINE_GLES)
static int TE_OS_WINDOW_GL_MAJOR_VERSION = 4; // same as GLAD version
static int TE_OS_WINDOW_GL_MINOR_VERSION = 5; // same as GLAD version
static int TE_OS_WINDOW_MSAA = 1; // set to 2 or 4 to require os to support msaa backbuffers
#else
static int TE_OS_WINDOW_GL_MAJOR_VERSION = 2; // same as GLAD version
static int TE_OS_WINDOW_GL_MINOR_VERSION = 0; // same as GLAD version
static int TE_OS_WINDOW_MSAA = 1;             // no MSAA under GLES
#endif
static int TE_OS_WINDOW_DEPTH_BITS = 24;
static int TE_OS_WINDOW_STENCIL_BITS = 8;

// all callbacks must be valid (specified as non-NULL value)
typedef struct te_os_window_callbacks {
    void (*on_mouse_button_pressed)(te_os_window* os_window, enum te_mouse_button button);
    void (*on_mouse_button_released)(te_os_window* os_window, enum te_mouse_button button);
    void (*on_mouse_move)(te_os_window* os_window, float x_diff, float y_diff);
    void (*on_mouse_scroll_moved)(te_os_window* os_window, float offset);
    void (*on_keyboard_button_pressed)(
        te_os_window* os_window, enum te_keyboard_button button, bool is_repeat,
        te_keyboard_modifiers modifiers);
    void (*on_keyboard_button_released)(
        te_os_window* os_window, enum te_keyboard_button button,
        te_keyboard_modifiers modifiers);
    void (*on_text_input)(
        te_os_window* os_window, const char* text); // utf-8, do not free the string
    void (*on_gamepad_connected)( // not called if gamepad is already connected when
        te_os_window* os_window); // window is being created (use getter to check if connected)
    void (*on_gamepad_disconnected)(te_os_window* os_window);
    void (*on_gamepad_button_pressed)(te_os_window* os_window, enum te_gamepad_button button);
    void (*on_gamepad_button_released)(te_os_window* os_window, enum te_gamepad_button button);
    void (*on_gamepad_axis_moved)(
        te_os_window* os_window, enum te_gamepad_axis axis, float new_pos);
    void (*on_received_focus)(te_os_window* os_window);
    void (*on_lost_focus)(te_os_window* os_window);
    void (*on_resized)(te_os_window* os_window, unsigned int width, unsigned int height);
} te_os_window_callbacks;

// Creates a new window depending on the current platform with the specified width and height in pixels.
// Also specify "user_data" object to query in callbacks using @ref os_window_get_user_data.
te_os_window*
os_window_create(const char* title, te_os_window_callbacks* callbacks, void* user_data);
void os_window_destroy(te_os_window* os_window);

// Call this function in your app loop and after it check for @ref os_window_should_close.
void os_window_poll_event(te_os_window* os_window);

void os_window_swap_buffers(struct te_os_window* os_window);

void os_window_capture_mouse_cursor(te_os_window* os_window, bool capture);

void* os_window_get_user_data(te_os_window* os_window);

// Returns the current size of the window in pixels.
void os_window_get_size(te_os_window* os_window, unsigned int* width, unsigned int* height);

unsigned int os_window_get_refresh_rate(te_os_window* os_window);

bool os_window_is_gamepad_connected(te_os_window* os_window);

// Sets/gets position of the cursor in pixels.
void os_window_set_cursor_position(te_os_window* os_window, float x, float y);
void os_window_get_cursor_position(te_os_window* os_window, float* x, float* y);

// Sets a flag that stops the window from processing window events.
void os_window_close(te_os_window* os_window);
bool os_window_should_close(te_os_window* os_window);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// sets platform-specific implementation data
void prv_os_window_set_impl(te_os_window* os_window, void* impl);
void* prv_os_window_get_impl(te_os_window* os_window);

void prv_os_window_on_size_changed(
    te_os_window* os_window, unsigned int width, unsigned int height);

te_os_window_callbacks* prv_os_window_get_callbacks(te_os_window* os_window);