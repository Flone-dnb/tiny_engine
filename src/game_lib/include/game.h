#pragma once

#include <input/gamepad_button.h>
#include <input/keyboard_button.h>
#include <input/mouse_button.h>

typedef struct te_game te_game;
struct te_game_manager;

te_game* game_create();
void game_destroy(te_game* game);

// window callbacks -------------------------------------------------------------------------------
void game_register_custom_types();
void game_on_game_started(void* game_instance, struct te_game_manager* game_manager);
void game_on_game_tick(
    void* game_instance, struct te_game_manager* game_manager, float delta_time_sec);
void game_on_keyboard_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_keyboard_button button,
    te_keyboard_modifiers modifiers);
void game_on_keyboard_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_keyboard_button button,
    te_keyboard_modifiers modifiers);
void game_on_keyboard_input_text(
    void* game_instance, struct te_game_manager* game_manager, const char* text);
void game_on_gamepad_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_button button);
void game_on_gamepad_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_button button);
void game_on_gamepad_axis_moved(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_axis axis,
    float new_pos);
void game_on_mouse_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_mouse_button button,
    bool was_handled_by_widget);
void game_on_mouse_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_mouse_button button,
    bool was_handled_by_widget);
void game_on_mouse_moved(
    void* game_instance, struct te_game_manager* game_manager, float x_offset, float y_offset);
void game_on_mouse_scroll_moved(
    void* game_instance, struct te_game_manager* game_manager, float offset);
void game_on_gamepad_connected(
    void* game_instance, struct te_game_manager* game_manager, const char* gamepad_name);
void game_on_gamepad_disconnected(void* game_instance, struct te_game_manager* game_manager);
void game_on_input_source_changed(
    void* game_instance, struct te_game_manager* game_manager, bool is_gamepad_current);
void
game_on_window_received_focus(void* game_instance, struct te_game_manager* game_manager);
void game_on_window_lost_focus(void* game_instance, struct te_game_manager* game_manager);
void game_on_window_close(void* game_instance, struct te_game_manager* game_manager);
// ------------------------------------------------------------------------------------------------
