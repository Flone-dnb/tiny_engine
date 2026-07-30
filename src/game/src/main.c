#include <io/log.h>
#if defined(WIN32)
// Hide console on Windows.
#pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup")
#endif

#include <stdlib.h>
#include <game.h>
#include <window.h>
#if defined(ENGINE_MEMCHECK_ENABLED)
#include <memcheck.h>
#endif

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

#if defined(ENGINE_MEMCHECK_ENABLED)
    memcheck_init();
#endif

    te_window* window = window_create("game");

    if (sizeof(te_window_callbacks) != sizeof(void*) * 18) {
        log_error("add new callbacks here");
        abort();
    }
    te_window_callbacks callbacks;
    callbacks.on_game_started = &game_on_game_started;
    callbacks.on_game_tick = &game_on_game_tick;
    callbacks.on_keyboard_button_pressed = &game_on_keyboard_button_pressed;
    callbacks.on_keyboard_button_released = &game_on_keyboard_button_released;
    callbacks.on_keyboard_input_text = &game_on_keyboard_input_text;
    callbacks.on_gamepad_button_pressed = &game_on_gamepad_button_pressed;
    callbacks.on_gamepad_button_released = &game_on_gamepad_button_released;
    callbacks.on_gamepad_axis_moved = &game_on_gamepad_axis_moved;
    callbacks.on_mouse_button_pressed = &game_on_mouse_button_pressed;
    callbacks.on_mouse_button_released = &game_on_mouse_button_released;
    callbacks.on_mouse_moved = &game_on_mouse_moved;
    callbacks.on_mouse_scroll_moved = &game_on_mouse_scroll_moved;
    callbacks.on_gamepad_connected = &game_on_gamepad_connected;
    callbacks.on_gamepad_disconnected = &game_on_gamepad_disconnected;
    callbacks.on_input_source_changed = &game_on_input_source_changed;
    callbacks.on_window_received_focus = &game_on_window_received_focus;
    callbacks.on_window_lost_focus = &game_on_window_lost_focus;
    callbacks.on_window_close = &game_on_window_close;

    te_game* game = game_create();
    window_process_events(window, &callbacks, game);
    game_destroy(game);

    window_destroy(window);

#if defined(ENGINE_MEMCHECK_ENABLED)
    memcheck_deinit();
#endif

    return 0;
}
