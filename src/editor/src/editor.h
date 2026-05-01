#pragma once

#include <input/gamepad_button.h>
#include <input/keyboard_button.h>
#include <input/mouse_button.h>
#include <ui/file_dialog.h>

typedef struct te_editor te_editor;
struct te_game_manager;
struct te_model;

te_editor* editor_create();
void editor_destroy(te_editor* editor);

struct te_game_manager* editor_get_game_manager(te_editor* editor);

// Destroys previous game world (if existed).
// Optionally specify a non-NULL path to file to load as the new world.
void editor_create_game_world(te_editor* editor, const char* relative_path_to_world);

// Shows a file dialog on top of everything.
// Specify NULL as `on_cancel` if you don't need it.
// Do not free the path variable passed to you in the callback.
void editor_show_file_dialog(
    te_editor* editor, void* custom, void (*on_selected)(void* custom, const char* path),
    void (*on_cancel)(void* custom), enum te_file_dialog_mode mode);

// Shows or hides gizmo around the specified model.
// Specify NULL to hide the gizmo.
void editor_set_gizmo(te_editor* editor, struct te_model* target);

// Called before a game object (for example: te_model) is deleted from the game world.
void editor_on_before_game_obj_deleted(te_editor* editor, void* game_obj);

// window callbacks -------------------------------------------------------------------------------
void editor_on_game_started(void* game_instance, struct te_game_manager* game_manager);
void editor_on_game_tick(
    void* game_instance, struct te_game_manager* game_manager, float delta_time_sec);
void editor_on_keyboard_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_keyboard_button button,
    te_keyboard_modifiers modifiers);
void editor_on_keyboard_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_keyboard_button button,
    te_keyboard_modifiers modifiers);
void editor_on_keyboard_input_text(
    void* game_instance, struct te_game_manager* game_manager, const char* text);
void editor_on_gamepad_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_button button);
void editor_on_gamepad_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_button button);
void editor_on_gamepad_axis_moved(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_axis axis,
    float new_pos);
void editor_on_mouse_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_mouse_button button,
    bool was_handled_by_widget);
void editor_on_mouse_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_mouse_button button,
    bool was_handled_by_widget);
void editor_on_mouse_moved(
    void* game_instance, struct te_game_manager* game_manager, float x_offset, float y_offset);
void editor_on_mouse_scroll_moved(
    void* game_instance, struct te_game_manager* game_manager, float offset);
void editor_on_gamepad_connected(
    void* game_instance, struct te_game_manager* game_manager, const char* gamepad_name);
void editor_on_gamepad_disconnected(void* game_instance, struct te_game_manager* game_manager);
void editor_on_input_source_changed(
    void* game_instance, struct te_game_manager* game_manager, bool is_gamepad_current);
void
editor_on_window_received_focus(void* game_instance, struct te_game_manager* game_manager);
void editor_on_window_lost_focus(void* game_instance, struct te_game_manager* game_manager);
void editor_on_window_close(void* game_instance, struct te_game_manager* game_manager);
// ------------------------------------------------------------------------------------------------
