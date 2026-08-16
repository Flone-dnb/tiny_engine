#include <game.h>

#include <render/renderer.h>
#include <render/font_manager.h>
#include <game_manager.h>
#include <world.h>
#include <camera_controller.h>
#include <game/camera.h>
#include <game/model.h>
#include <window.h>

struct te_game {
    // NULL if the game is not started yet. Do not free/destroy this pointer, valid while the game exists.
    te_game_manager* game_manager;

    // Always valid.
    te_camera_controller* camera_controller;
};

te_game*
game_create() {
    te_game* game = malloc(sizeof(te_game));
    game->camera_controller = camera_controller_create();
    game->game_manager = NULL;

    return game;
}

void
game_destroy(te_game* game) {
    camera_controller_destroy(game->camera_controller);

    free(game);
}

void
game_register_custom_types() {
    // (if you have any) register custom types in the reflection database here,
    // the editor calls this function and will be able to see your custom types
}

void
game_on_game_started(void* game_instance, te_game_manager* game_manager) {
    game_register_custom_types();

    te_game* game = game_instance;
    game->game_manager = game_manager;

    // Load engine default font.
    te_renderer* renderer = game_manager_get_renderer(game_manager);
    te_font_manager* font_manager = renderer_get_font_manager(renderer);
    font_manager_load_font(font_manager, "engine/font/font.ttf");

    // Create a sample world.
    te_world* world = game_manager_create_world(game_manager, "game");

    // Spawn active camera.
    te_camera* camera = camera_create();
    world_spawn_game_object(world, camera, camera_get_game_object_info());
    world_set_active_camera(world, camera);

    camera_controller_set_camera(game->camera_controller, camera);
    window_capture_mouse_cursor(game_manager_get_window(game_manager), true);

    // Set initial camera position/rotation.
    camera_set_position(camera, (vec3){0.0f, 2.0f, 4.0f});
    camera_set_rotation(camera, (vec3){0.0f, 0.0f, 0.0f});

    // Setup light.
    te_light_params* light_params =
        renderer_get_light_params(game_manager_get_renderer(game_manager));
    glm_vec3_copy((vec3){1.0f, -1.0f, 1.0f}, light_params->directional_light_direction);
    glm_vec3_normalize(light_params->directional_light_direction);
    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, light_params->directional_light_color);

    // Prepare a sample scene.
    te_model* floor = model_create();
    model_set_name(floor, "floor");
    model_set_scale(floor, (vec3){4.0f, 1.0f, 4.0f});
    model_set_color(floor, (vec4){1.0f, 0.5f, 0.0f, 1.0f});
    world_spawn_game_object(world, floor, model_get_game_object_info());

    te_model* box = model_create();
    model_set_name(box, "box");
    model_set_position(box, (vec3){0.0f, 1.0f, -1.0f});
    world_spawn_game_object(world, box, model_get_game_object_info());
}

void
game_on_window_close(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_instance;
    (void)game_manager;
}

void
game_on_game_tick(void* game_instance, te_game_manager* game_manager, float delta_time_sec) {
    (void)game_manager;
    (void)delta_time_sec;

    te_game* game = game_instance;
    camera_controller_on_game_tick(game->camera_controller, delta_time_sec);
}

void
game_on_keyboard_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_keyboard_button button,
    te_keyboard_modifiers modifiers) {
    (void)game_manager;
    (void)modifiers;

    te_game* game = game_instance;
    camera_controller_on_keyboard_button_pressed(game->camera_controller, button);
}

void
game_on_keyboard_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_keyboard_button button,
    te_keyboard_modifiers modifiers) {
    (void)game_manager;
    (void)modifiers;

    te_game* game = game_instance;
    camera_controller_on_keyboard_button_released(game->camera_controller, button);
}

void
game_on_keyboard_input_text(
    void* game_instance, struct te_game_manager* game_manager, const char* text) {
    (void)game_instance;
    (void)game_manager;
    (void)text;
}

void
game_on_gamepad_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_button button) {
    (void)game_instance;
    (void)game_manager;
    (void)button;
}

void
game_on_gamepad_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_button button) {
    (void)game_instance;
    (void)game_manager;
    (void)button;
}

void
game_on_gamepad_axis_moved(
    void* game_instance, struct te_game_manager* game_manager, enum te_gamepad_axis axis,
    float new_pos) {
    (void)game_manager;

    te_game* game = game_instance;
    camera_controller_on_gamepad_axis_moved(game->camera_controller, axis, new_pos);
}

void
game_on_mouse_button_pressed(
    void* game_instance, struct te_game_manager* game_manager, enum te_mouse_button button,
    bool was_handled_by_widget) {
    (void)game_instance;
    (void)game_manager;
    (void)button;
    (void)was_handled_by_widget;
}

void
game_on_mouse_button_released(
    void* game_instance, struct te_game_manager* game_manager, enum te_mouse_button button,
    bool was_handled_by_widget) {
    (void)game_instance;
    (void)game_manager;
    (void)button;
    (void)was_handled_by_widget;
}

void
game_on_mouse_moved(
    void* game_instance, struct te_game_manager* game_manager, float x_offset,
    float y_offset) {
    (void)game_manager;

    te_game* game = game_instance;
    camera_controller_on_mouse_moved(game->camera_controller, x_offset, y_offset);
}

void
game_on_mouse_scroll_moved(
    void* game_instance, struct te_game_manager* game_manager, float offset) {
    (void)game_instance;
    (void)game_manager;
    (void)offset;
}

void
game_on_gamepad_connected(
    void* game_instance, struct te_game_manager* game_manager, const char* gamepad_name) {
    (void)game_manager;
    (void)gamepad_name;

    te_game* game = game_instance;
    camera_controller_on_gamepad_connected(game->camera_controller);
}

void
game_on_gamepad_disconnected(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_manager;

    te_game* game = game_instance;
    camera_controller_on_gamepad_disconnected(game->camera_controller);
}

void
game_on_input_source_changed(
    void* game_instance, struct te_game_manager* game_manager, bool is_gamepad_current) {
    (void)game_instance;
    (void)game_manager;
    (void)is_gamepad_current;
}

void
game_on_window_received_focus(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_instance;
    (void)game_manager;
}

void
game_on_window_lost_focus(void* game_instance, struct te_game_manager* game_manager) {
    (void)game_instance;
    (void)game_manager;
}
