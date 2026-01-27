#include "editor.h"

#include "game/camera.h"
#include "game/model.h"
#include "game_manager.h"
#include "world.h"

void
editor_on_game_started(struct te_game_manager* game_manager) {
    te_world* world = game_manager_create_world(game_manager, "game");

    te_camera* camera = camera_create();
    camera_set_location(camera, (vec3){0.0f, 0.0f, 3.0f});

    world_spawn_camera(world, camera);
    world_set_active_camera(world, camera);

    te_model* model = model_create(NULL);
    world_spawn_model(world, model);
}

void
editor_on_game_tick(struct te_game_manager* game_manager, float delta_time_sec) {
    (void)game_manager;
    (void)delta_time_sec;
    // TODO
}

void
editor_on_window_close(struct te_game_manager* game_manager) {
    (void)game_manager;
}
