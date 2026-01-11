#include "game_manager.h"

#include <stdlib.h>
#include "renderer.h"

te_game_manager*
game_manager_create(struct te_window* window,
                    void (*on_game_tick)(te_game_manager* game_manager, float delta_time_sec)) {
    te_game_manager* game_manager = (te_game_manager*)malloc(sizeof(te_game_manager));

    game_manager->window = window;
    game_manager->on_game_tick = on_game_tick;

    game_manager->renderer = (te_renderer*)malloc(sizeof(te_renderer));

    return game_manager;
}

void
game_manager_destroy(te_game_manager* game_manager) {
    renderer_destroy(game_manager->renderer);

    free(game_manager);
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
