#pragma once

#include <stdbool.h>

#include <input/gamepad_button.h>
#include <input/keyboard_button.h>
#include <input/mouse_button.h>

typedef struct te_window te_window;

struct te_game_manager;

/* initial callbacks that the user must specify
 * all must be non-NULL */
typedef struct te_window_callbacks {
    /* called when engine initialized everything and the game is ready to start */
    void (*on_game_started)(void* game_instance, struct te_game_manager* game_manager);

    /* called before a new frame is rendered */
    void (*on_game_tick)(
        void* game_instance, struct te_game_manager* game_manager, float delta_time_sec);

    void (*on_keyboard_button_pressed)(
        void* game_instance, struct te_game_manager* game_manager,
        enum te_keyboard_button button, te_keyboard_modifiers modifiers);
    void (*on_keyboard_button_released)(
        void* game_instance, struct te_game_manager* game_manager,
        enum te_keyboard_button button, te_keyboard_modifiers modifiers);

    /* called when a text input event is received. Generally happens when typing text in a "text edit" widget
     * text is a UTF-8 encoded string. Do not free the text pointer */
    void (*on_keyboard_input_text)(
        void* game_instance, struct te_game_manager* game_manager, const char* text);

    void (*on_gamepad_button_pressed)(
        void* game_instance, struct te_game_manager* game_manager,
        enum te_gamepad_button button);
    void (*on_gamepad_button_released)(
        void* game_instance, struct te_game_manager* game_manager,
        enum te_gamepad_button button);

    /* called when the window receives gamepad input. New pos in range [-1.0f; 1.0f] */
    void (*on_gamepad_axis_moved)(
        void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_axis axis,
        float new_pos);

    void (*on_mouse_button_pressed)(
        void* game_instance, struct te_game_manager* game_manager, enum te_mouse_button button,
        bool was_handled_by_widget);
    void (*on_mouse_button_released)(
        void* game_instance, struct te_game_manager* game_manager, enum te_mouse_button button,
        bool was_handled_by_widget);
    void (*on_mouse_moved)(
        void* game_instance, struct te_game_manager* game_manager, float x_offset,
        float y_offset);
    void (*on_mouse_scroll_moved)(
        void* game_instance, struct te_game_manager* game_manager, float offset);

    /* called when a gamepad connects. Gamepad name string must not be freed/destroyed */
    void (*on_gamepad_connected)(void* game_instance, struct te_game_manager* game_manager);
    void (*on_gamepad_disconnected)(void* game_instance, struct te_game_manager* game_manager);

    /* called after the input device changed (keyboard+mouse/gamepad) */
    void (*on_input_source_changed)(
        void* game_instance, struct te_game_manager* game_manager, bool is_gamepad_current);

    void (*on_window_received_focus)(
        void* game_instance, struct te_game_manager* game_manager);
    void (*on_window_lost_focus)(void* game_instance, struct te_game_manager* game_manager);

    /* called before the window is closed (before the game is closed) */
    void (*on_window_close)(void* game_instance, struct te_game_manager* game_manager);
} te_window_callbacks;

te_window* window_create(const char* window_title);
void window_destroy(te_window* window);

/* returns game manager
 * always valid pointer to the game manager, do not free/destroy returned pointer
 * valid while @ref window_process_events (i.e. the game) is running */
struct te_game_manager* window_get_game_manager(te_window* window);

/* captures the mouse cursor to be inside of the window */
void window_capture_mouse_cursor(te_window* window, bool enable);

bool window_is_mouse_captured(te_window* window);
bool window_is_gamepad_connected(te_window* window);

/* sets a flag that stops the window from processing window events
 * that were initiated by calling @ref window_process_events
 * use this function to close the game */
void window_close(te_window* window);

/* returns the current size of the window (in pixels) */
void window_get_size(te_window* window, unsigned int* width, unsigned int* height);

/* sets/gets position of the cursor in pixels */
void window_get_cursor_position(te_window* window, float* x, float* y);
void window_set_cursor_position(te_window* window, float x, float y);

unsigned int window_get_display_refresh_rate(te_window* window);

/* runs the window's event loop
 * specify callbacks and a pointer to the game's main system, this pointer will be passed to various callbacks */
void window_process_events(
    te_window* window, te_window_callbacks* window_callbacks, void* game_instance);

/* ------------------------------------------------------------------------------------------------
 *                                       PRIVATE API
 * ------------------------------------------------------------------------------------------------ */

/* returns user's main game system */
void* prv_window_get_game_instance(te_window* window);

void prv_window_swap_buffers(te_window* window);
