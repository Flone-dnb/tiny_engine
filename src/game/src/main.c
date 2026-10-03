#include <io/log.h>
#if defined(WIN32)
/* hide console on Windows */
#pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup")
#endif

#include <stdlib.h>
#include <game.h>
#include <window.h>
#if defined(ENGINE_MEMCHECK_ENABLED)
#include <memcheck.h>
#endif

int
main(void) {
    te_window* window;
    te_game* game;
    te_window_callbacks callbacks;

#if defined(ENGINE_MEMCHECK_ENABLED)
    memcheck_init();
#endif

    window = window_create("game");

    if (sizeof(te_window_callbacks) != sizeof(void*) * 18) {
        log_error(__FILE__, __LINE__, "add new callbacks here");
        abort();
    }
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

    game = game_create();
    window_process_events(window, &callbacks, game);
    game_destroy(game);

    window_destroy(window);

#if defined(ENGINE_MEMCHECK_ENABLED)
    memcheck_deinit();
#endif

    return 0;
}
