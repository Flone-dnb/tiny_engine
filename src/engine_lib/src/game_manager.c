#include "game_manager.h"

#include <stdlib.h>
#include "renderer.h"
#include "world.h"

te_game_manager*
game_manager_create(struct te_window* window,
                    void (*on_game_tick)(te_game_manager* game_manager, float delta_time_sec)) {
    te_game_manager* game_manager = (te_game_manager*)malloc(sizeof(te_game_manager));

    game_manager->window = window;
    game_manager->on_game_tick = on_game_tick;

    game_manager->worlds = NULL;
    game_manager->world_count = 0;

    game_manager->renderer = renderer_create(window);

    return game_manager;
}

void
game_manager_destroy(te_game_manager* game_manager) {
    // First destroy worlds.
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        prv_world_destroy(game_manager->worlds[i]);
    }
    free(game_manager->worlds);

    renderer_destroy(game_manager->renderer);

    free(game_manager);
}

void
game_manager_create_world(te_game_manager* game_manager, const char* name) {
    te_world** new_worlds = malloc(sizeof(te_world*) * (game_manager->world_count + 1u));

    // Copy old pointers.
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        new_worlds[i] = game_manager->worlds[i];
    }

    // Create new world.
    new_worlds[game_manager->world_count] = prv_world_create(game_manager, name);
    game_manager->world_count += 1;

    // Save new array.
    free(game_manager->worlds);
    game_manager->worlds = new_worlds;
}

struct te_window*
game_manager_get_window(te_game_manager* game_manager) {
    return game_manager->window;
}

void
prv_game_manager_tick(te_game_manager* game_manager, float delta_time_sec) {
    game_manager->on_game_tick(game_manager, delta_time_sec);
}

void
prv_game_manager_draw_frame(te_game_manager* game_manager) {
    prv_renderer_draw_frame(game_manager->renderer);
}

void
prv_game_manager_on_window_size_changed(te_game_manager* game_manager) {
    prv_renderer_on_window_size_changed(game_manager->renderer);
}
