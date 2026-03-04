#include <game_manager.h>

#include <stdlib.h>
#include <string.h>
#include <cglm/vec2.h>
#include <debug_console.h>
#include <io/log.h>
#include <render/debug_drawer.h>
#include <render/renderer.h>
#include <type_database.h>
#include <window.h>
#include <world.h>

// Stores all core systems such as game world, physics, audio, renderer and etc.
struct te_game_manager {
    // Always valid pointer to the window that owns this object. This pointer should not be freed.
    struct te_window* window;

    te_renderer* renderer;

    // Game worlds. Size of this array is @ref world_count.
    te_world** worlds;

    // Number of elements in the @ref worlds array.
    unsigned int world_count;
};

te_game_manager*
prv_game_manager_create(struct te_window* window) {
    te_game_manager* game_manager = (te_game_manager*)malloc(sizeof(te_game_manager));

    game_manager->window = window;
    game_manager->worlds = NULL;
    game_manager->world_count = 0;

    prv_type_database_init();

#if defined(ENGINE_DEBUG_TOOLS)
    prv_debug_console_init(game_manager);
#endif

    game_manager->renderer = renderer_create(window);

#if defined(ENGINE_DEBUG_TOOLS)
    prv_debug_drawer_init(game_manager->renderer);
#endif

#if defined(ENGINE_ASAN_ENABLED)
    log_info("AddressSanitizer (ASan) is enabled, expect increased RAM usage!");
#endif

#if defined(DEBUG)
    log_info("DEBUG macro is defined, running debug build");
#else
    log_info("DEBUG macro is NOT defined, running release build");
#endif

#if defined(ENGINE_DEBUG_TOOLS)
    log_info("ENGINE_DEBUG_TOOLS macro is defined, debug tools are enabled");
#else
    log_info("ENGINE_DEBUG_TOOLS macro is NOT defined");
#endif

    return game_manager;
}

void
prv_game_manager_destroy(te_game_manager* game_manager) {
    // First destroy worlds.
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        prv_world_destroy(game_manager->worlds[i]);
    }
    free(game_manager->worlds);

#if defined(ENGINE_DEBUG_TOOLS)
    // Destroy before renderer.
    prv_debug_console_deinit();
    prv_debug_drawer_deinit(game_manager->renderer);
#endif

    renderer_destroy(game_manager->renderer);

    prv_type_database_deinit();

    free(game_manager);
}

te_world*
game_manager_create_world(te_game_manager* game_manager, const char* name) {
    if (name == NULL) {
        log_error("world name must not be NULL");
        abort();
    }

    // Expand array.
    te_world** new_worlds = malloc(sizeof(te_world*) * (game_manager->world_count + 1));
    memcpy(new_worlds, game_manager->worlds, sizeof(te_world*) * game_manager->world_count);

    free(game_manager->worlds);
    game_manager->worlds = new_worlds;

    // Create new world.
    te_world* new_world = prv_world_create(game_manager, name);
    game_manager->worlds[game_manager->world_count] = new_world;
    game_manager->world_count += 1;

    return new_world;
}

void
game_manager_destroy_world(te_game_manager* game_manager, te_world* world) {
    if (game_manager->world_count == 1) {
        game_manager->world_count = 0;
        prv_world_destroy(game_manager->worlds[0]);
        free(game_manager->worlds);
        game_manager->worlds = NULL;
    } else {
        bool found = false;
        unsigned int index = 0;
        for (unsigned int i = 0; i < game_manager->world_count; i++) {
            if (game_manager->worlds[i] != world) {
                continue;
            }
            found = true;
            index = i;
            break;
        }
        if (!found) {
            log_error("unable to find the specified world to destroy");
            abort();
        }

        prv_world_destroy(game_manager->worlds[index]);

        te_world** new_worlds = malloc(sizeof(te_world*) * (game_manager->world_count - 1));
        memcpy(new_worlds, game_manager->worlds, sizeof(te_world*) * index);
        memcpy(
            new_worlds + index, game_manager->worlds + (index + 1), sizeof(te_world*) * (game_manager->world_count - index - 1));

        free(game_manager->worlds);
        game_manager->worlds = new_worlds;

        game_manager->world_count -= 1;
    }
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

void*
game_manager_get_game_instance(te_game_manager* game_manager) {
    return prv_window_get_game_instance(game_manager->window);
}

void
prv_game_manager_tick(te_game_manager* game_manager, float delta_time_sec) {
    (void)game_manager;
    (void)delta_time_sec;
    // TODO
}

void
prv_game_manager_draw_frame(te_game_manager* game_manager, float delta_time_sec) {
    prv_renderer_draw_frame(game_manager->renderer, delta_time_sec);
}

void
prv_game_manager_on_window_size_changed(te_game_manager* game_manager) {
    prv_renderer_on_window_size_changed(game_manager->renderer);

    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        prv_world_on_window_size_changed(game_manager->worlds[i]);
    }
}

bool
prv_game_manager_on_mouse_button_pressed(te_game_manager* game_manager, enum te_mouse_button button) {
    vec2 cursor_pos;
    window_get_cursor_position(game_manager->window, &cursor_pos[0], &cursor_pos[1]);

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(game_manager->window, &window_width, &window_height);

    glm_vec2_div(cursor_pos, (vec2){(float)window_width, (float)window_height}, cursor_pos);

    bool is_handled = false;
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        is_handled |= prv_world_on_mouse_button_pressed(game_manager->worlds[i], button, cursor_pos);
    }

    return is_handled;
}

bool
prv_game_manager_on_mouse_button_released(te_game_manager* game_manager, enum te_mouse_button button) {
    vec2 cursor_pos;
    window_get_cursor_position(game_manager->window, &cursor_pos[0], &cursor_pos[1]);

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(game_manager->window, &window_width, &window_height);

    glm_vec2_div(cursor_pos, (vec2){(float)window_width, (float)window_height}, cursor_pos);

    bool is_handled = false;
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        is_handled |= prv_world_on_mouse_button_released(game_manager->worlds[i], button, cursor_pos);
    }

    return is_handled;
}

void
prv_game_manager_on_mouse_moved(te_game_manager* game_manager) {
    vec2 cursor_pos;
    window_get_cursor_position(game_manager->window, &cursor_pos[0], &cursor_pos[1]);

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(game_manager->window, &window_width, &window_height);

    glm_vec2_div(cursor_pos, (vec2){(float)window_width, (float)window_height}, cursor_pos);

    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        prv_world_on_mouse_moved(game_manager->worlds[i], cursor_pos);
    }
}

void
prv_game_manager_on_keyboard_input_text(te_game_manager* game_manager, const char* text) {
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        prv_world_on_keyboard_input_text(game_manager->worlds[i], text);
    }
}

void
prv_game_manager_on_keyboard_input(te_game_manager* game_manager, enum te_keyboard_button button, bool is_repeat) {
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        prv_world_on_keyboard_input(game_manager->worlds[i], button, is_repeat);
    }
}

void
prv_game_manager_on_input_source_changed(te_game_manager* game_manager) {
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        prv_world_on_input_source_changed(game_manager->worlds[i]);
    }
}
