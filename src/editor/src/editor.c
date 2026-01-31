#include "editor.h"

#include "editor_camera.h"
#include "game/model.h"
#include "game_manager.h"
#include "window.h"
#include "world.h"

/** Main editor object. */
struct te_editor {
    /** Always valid pointer. Must be destroyed during the editor's destruction. */
    te_editor_camera* editor_camera;

    /** Not NULL if game world exists. */
    te_world* game_world;
};

te_editor*
editor_create() {
    te_editor* editor = malloc(sizeof(te_editor));
    editor->editor_camera = editor_camera_create();
    editor->game_world = NULL;

    return editor;
}

void
editor_destroy(te_editor* editor) {
    editor_camera_destroy(editor->editor_camera);

    free(editor);
}

void
editor_on_game_started(void* game_instance, te_game_manager* game_manager) {
    te_editor* editor = game_instance;
    editor_create_game_world(editor, game_manager);
}

void
editor_destroy_game_world(te_editor* editor, te_game_manager* game_manager) {
    // Despawn editor camera because we manage its destruction manually.
    editor_camera_despawn(editor->editor_camera, editor->game_world);

    // Destroy world.
    game_manager_destroy_world(game_manager, editor->game_world);
    editor->game_world = NULL;
}

void
editor_create_game_world(te_editor* editor, te_game_manager* game_manager) {
    if (editor->game_world != NULL) {
        editor_destroy_game_world(editor, game_manager);
    }

    editor->game_world = game_manager_create_world(game_manager, "game");
    editor_camera_spawn(editor->editor_camera, editor->game_world);

    te_model* model = model_create(NULL);
    world_spawn_model(editor->game_world, model);
}

void
editor_on_game_tick(void* game_instance, struct te_game_manager* game_manager, float delta_time_sec) {
    (void)game_manager;

    te_editor* editor = game_instance;
    editor_camera_on_game_tick(editor->editor_camera, delta_time_sec);
}

void
editor_on_keyboard_button_pressed(void* game_instance, struct te_game_manager* game_manager,
                                  enum te_keyboard_button button, te_keyboard_modifiers modifiers) {
    (void)game_manager;
    (void)modifiers;

    te_editor* editor = game_instance;
    editor_camera_on_keyboard_button_pressed(editor->editor_camera, button);
}

void
editor_on_keyboard_button_released(void* game_instance, struct te_game_manager* game_manager,
                                   enum te_keyboard_button button, te_keyboard_modifiers modifiers) {
    (void)game_manager;
    (void)modifiers;

    te_editor* editor = game_instance;
    editor_camera_on_keyboard_button_released(editor->editor_camera, button);
}

void
editor_on_gamepad_button_pressed(void* game_instance, struct te_game_manager* game_manager,
                                 enum te_gamepad_button button) {
    (void)game_instance;
    (void)game_manager;
    (void)button;
}

void
editor_on_gamepad_button_released(void* game_instance, struct te_game_manager* game_manager,
                                  enum te_gamepad_button button) {
    (void)game_instance;
    (void)game_manager;
    (void)button;
}

void
editor_on_gamepad_axis_moved(void* game_instance, struct te_game_manager* game_manager,
                             enum te_gamepad_axis axis, float new_pos) {
    (void)game_manager;

    te_editor* editor = game_instance;
    editor_camera_on_gamepad_axis_moved(editor->editor_camera, axis, new_pos);
}

void
editor_on_mouse_button_pressed(void* game_instance, struct te_game_manager* game_manager,
                               enum te_mouse_button button) {
    te_editor* editor = game_instance;

    if (button == TE_MB_RIGHT) {
        if (editor->game_world == NULL) {
            // No point in doing something.
            return;
        }

        te_window* window = game_manager_get_window(game_manager);
        window_capture_mouse_cursor(window, true);

        editor_camera_enable_input(editor->editor_camera, true);
    }
}

void
editor_on_mouse_button_released(void* game_instance, struct te_game_manager* game_manager,
                                enum te_mouse_button button) {
    te_editor* editor = game_instance;

    if (button == TE_MB_RIGHT) {
        if (editor->game_world == NULL) {
            // No point in doing something.
            return;
        }

        te_window* window = game_manager_get_window(game_manager);
        window_capture_mouse_cursor(window, false);

        editor_camera_enable_input(editor->editor_camera, false);
    }
}

void
editor_on_mouse_moved(void* game_instance, struct te_game_manager* game_manager, float x_offset,
                      float y_offset) {
    (void)game_manager;

    te_editor* editor = game_instance;
    editor_camera_on_mouse_moved(editor->editor_camera, x_offset, y_offset);
}

void
editor_on_mouse_scroll_moved(void* game_instance, struct te_game_manager* game_manager, float offset) {
    (void)game_instance;
    (void)game_manager;
    (void)offset;
}

void
editor_on_gamepad_connected(void* game_instance, struct te_game_manager* game_manager,
                            const char* gamepad_name) {
    (void)game_manager;
    (void)gamepad_name;

    te_editor* editor = game_instance;
    editor_camera_on_gamepad_connected(editor->editor_camera);
}

void
editor_on_gamepad_disconnected(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_manager;

    te_editor* editor = game_instance;
    editor_camera_on_gamepad_disconnected(editor->editor_camera);
}

void
editor_on_input_source_changed(void* game_instance, struct te_game_manager* game_manager,
                               bool is_gamepad_current) {
    (void)game_instance;
    (void)game_manager;
    (void)is_gamepad_current;
}

void
editor_on_window_received_focus(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_instance;
    (void)game_manager;
}

void
editor_on_window_lost_focus(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_manager;

    te_editor* editor = game_instance;
    if (editor->game_world == NULL) {
        // No point in doing something.
        return;
    }

    te_window* window = game_manager_get_window(game_manager);
    window_capture_mouse_cursor(window, false);

    editor_camera_enable_input(editor->editor_camera, false);
}

void
editor_on_window_close(void* game_instance, struct te_game_manager* game_manager) {
    te_editor* editor = game_instance;

    if (editor->game_world != NULL) {
        editor_destroy_game_world(editor, game_manager);
    }
}
