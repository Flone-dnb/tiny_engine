#include "misc/error.h"
#if defined(WIN32)
// Hide console on Windows.
#pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup")
#endif

#include <stdlib.h>
#include "editor.h"
#include "window.h"

#if defined(WIN32)
#include <Windows.h>
#include <crtdbg.h>
#endif

int
main(void) {
    // Enable run-time memory checks for debug builds (on Windows).
#if defined(WIN32) && defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    te_window* window = window_create("tiny engine editor");

    if (sizeof(te_window_callbacks) != sizeof(void*) * 17) {
        show_error_and_abort("add new callbacks here");
    }
    te_window_callbacks callbacks;
    callbacks.on_game_started = &editor_on_game_started;
    callbacks.on_game_tick = &editor_on_game_tick;
    callbacks.on_keyboard_button_pressed = &editor_on_keyboard_button_pressed;
    callbacks.on_keyboard_button_released = &editor_on_keyboard_button_released;
    callbacks.on_gamepad_button_pressed = &editor_on_gamepad_button_pressed;
    callbacks.on_gamepad_button_released = &editor_on_gamepad_button_released;
    callbacks.on_gamepad_axis_moved = &editor_on_gamepad_axis_moved;
    callbacks.on_mouse_button_pressed = &editor_on_mouse_button_pressed;
    callbacks.on_mouse_button_released = &editor_on_mouse_button_released;
    callbacks.on_mouse_moved = &editor_on_mouse_moved;
    callbacks.on_mouse_scroll_moved = &editor_on_mouse_scroll_moved;
    callbacks.on_gamepad_connected = &editor_on_gamepad_connected;
    callbacks.on_gamepad_disconnected = &editor_on_gamepad_disconnected;
    callbacks.on_input_source_changed = &editor_on_input_source_changed;
    callbacks.on_window_received_focus = &editor_on_window_received_focus;
    callbacks.on_window_lost_focus = &editor_on_window_lost_focus;
    callbacks.on_window_close = &editor_on_window_close;

    te_editor* editor = editor_create();
    window_process_events(window, &callbacks, editor);
    editor_destroy(editor);

    window_destroy(window);

    return 0;
}
