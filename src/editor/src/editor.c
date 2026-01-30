#include "editor.h"

#include "game/camera.h"
#include "game/model.h"
#include "game_manager.h"
#include "world.h"

void
editor_on_game_started(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_instance;

    te_world* world = game_manager_create_world(game_manager, "game");

    te_camera* camera = camera_create();
    camera_set_location(camera, (vec3){0.0f, 0.0f, 3.0f});

    world_spawn_camera(world, camera);
    world_set_active_camera(world, camera);

    te_model* model = model_create(NULL);
    world_spawn_model(world, model);
}

void
editor_on_game_tick(void* game_instance, struct te_game_manager* game_manager, float delta_time_sec) {
    (void)game_instance;
    (void)game_manager;
    (void)delta_time_sec;
}

void
editor_on_keyboard_button_pressed(void* game_instance, struct te_game_manager* game_manager,
                                  enum te_keyboard_button button, te_keyboard_modifiers modifiers) {
    (void)game_instance;
    (void)game_manager;
    (void)button;
    (void)modifiers;
}

void
editor_on_keyboard_button_released(void* game_instance, struct te_game_manager* game_manager,
                                   enum te_keyboard_button button, te_keyboard_modifiers modifiers) {
    (void)game_instance;
    (void)game_manager;
    (void)button;
    (void)modifiers;
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
    (void)game_instance;
    (void)game_manager;
    (void)axis;
    (void)new_pos;
}

void
editor_on_mouse_button_pressed(void* game_instance, struct te_game_manager* game_manager,
                               enum te_mouse_button button) {
    (void)game_instance;
    (void)game_manager;
    (void)button;
}

void
editor_on_mouse_button_released(void* game_instance, struct te_game_manager* game_manager,
                                enum te_mouse_button button) {
    (void)game_instance;
    (void)game_manager;
    (void)button;
}

void
editor_on_mouse_moved(void* game_instance, struct te_game_manager* game_manager, float x_offset,
                      float y_offset) {
    (void)game_instance;
    (void)game_manager;
    (void)x_offset;
    (void)y_offset;
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
    (void)game_instance;
    (void)game_manager;
    (void)gamepad_name;
}

void
editor_on_gamepad_disconnected(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_instance;
    (void)game_manager;
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
    (void)game_instance;
    (void)game_manager;
}

void
editor_on_window_close(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_instance;
    (void)game_manager;
}
