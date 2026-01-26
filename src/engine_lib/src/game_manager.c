#include "game_manager.h"

#include <stdlib.h>
#include "render/renderer.h"
#include "world.h"

/** Stores all core systems such as game world, physics, audio, renderer and etc. */
struct te_game_manager {
    /** Always valid pointer to the window that owns this object. This pointer should not be freed. */
    struct te_window* window;

    /** Renderer. */
    struct te_renderer* renderer;

    /** Game worlds. Size of this array is @ref world_count. */
    struct te_world** worlds;

    /** User callback that should be called on game tick. */
    void (*on_game_tick)(struct te_game_manager* game_manager, float delta_time_sec);

    /** Number of elements in the @ref worlds array. */
    unsigned int world_count;
};

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

te_renderer*
game_manager_get_renderer(te_game_manager* game_manager) {
    return game_manager->renderer;
}

struct te_world**
game_manager_get_worlds(te_game_manager* game_manager, unsigned int* world_count) {
    *world_count = game_manager->world_count;
    return game_manager->worlds;
}

void
prv_game_manager_tick(te_game_manager* game_manager, float delta_time_sec) {
    // Trigger user callback.
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
