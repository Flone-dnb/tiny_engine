#include <game_manager.h>

#include <stdlib.h>
#include <string.h>
#include <cglm/vec2.h>
#include <debug_console.h>
#include <io/log.h>
#include <render/debug_drawer.h>
#include <render/renderer.h>
#include <game/camera.h>
#include <type_database.h>
#include <window.h>
#include <world.h>
#include <sound_manager.h>

typedef struct te_game_tick_callback {
    void* custom;
    void (*on_tick)(void* custom, float delta_time_sec);
    unsigned int id;
} te_game_tick_callback;

// Stores all core systems such as game world, physics, audio, renderer and etc.
struct te_game_manager {
    // Always valid pointer to the window that owns this object. This pointer should not be freed.
    struct te_window* window;

    te_renderer* renderer;

    te_sound_manager* sound_manager;

    // Custom user tick callbacks. The number of elements in this array is @ref tick_callback_count.
    te_game_tick_callback* tick_callbacks;

    // Game worlds. Size of this array is @ref world_count.
    te_world** worlds;

    // Number of elements in the @ref worlds array.
    unsigned int world_count;

    // Number of elements in the @ref tick_callbacks array.
    unsigned int tick_callback_count;

    // Next unique ID of a new tick callback to be used.
    unsigned int next_tick_callback_id;

    // `true` if we are currently iterating over @ref tick_callbacks.
    bool is_processing_tick_callbacks;

    // `true` if while processing tick callbacks they changed.
    bool is_tick_callbacks_changed;
};

te_game_manager*
prv_game_manager_create(struct te_window* window) {
    te_game_manager* game_manager = malloc(sizeof(te_game_manager));

    game_manager->sound_manager = sound_manager_create();
    game_manager->window = window;
    game_manager->worlds = NULL;
    game_manager->world_count = 0;
    game_manager->tick_callbacks = NULL;
    game_manager->tick_callback_count = 0;
    game_manager->next_tick_callback_id = 0;
    game_manager->is_processing_tick_callbacks = false;
    game_manager->is_tick_callbacks_changed = false;

    prv_type_database_init();

#if defined(ENGINE_DEBUG_TOOLS)
    prv_debug_console_init(game_manager);
#endif

    game_manager->renderer = renderer_create(window);

#if defined(ENGINE_DEBUG_TOOLS)
    prv_debug_drawer_init(game_manager->renderer);
#endif

    // Log some info.
    log_info("state:");

#if defined(ENGINE_ASAN_ENABLED)
    log_info("- AddressSanitizer (ASan) is enabled, expect increased RAM usage!");
#endif

#if defined(DEBUG)
    log_info("- DEBUG is defined, running debug build");
#else
    log_info("- DEBUG is NOT defined, running release build");
#endif

#if defined(ENGINE_DEBUG_TOOLS)
    log_info("- ENGINE_DEBUG_TOOLS is defined, debug tools are enabled");
#else
    log_info("- ENGINE_DEBUG_TOOLS is NOT defined");
#endif

#if defined(ENGINE_MEMCHECK_ENABLED)
#if !defined(DEBUG)
#error "memcheck should be disabled in release builds"
#endif
    log_info("- ENGINE_MEMCHECK_ENABLED is defined, memcheck enabled");
#else
    log_info("- ENGINE_MEMCHECK_ENABLED is NOT defined");
#endif

#if defined(ENGINE_GLES)
    log_info("- ENGINE_GLES is defined, using OpenGL ES");
#else
    log_info("- ENGINE_GLES is NOT defined, using regular OpenGL");
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
    sound_manager_destroy(game_manager->sound_manager);

    prv_type_database_deinit();

    if (game_manager->tick_callback_count > 0) {
        log_error_fmt(
            "the game manager is destroyed but there are still %u tick callback(s) registered",
            game_manager->tick_callback_count);
        abort();
    }

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
            new_worlds + index, game_manager->worlds + (index + 1),
            sizeof(te_world*) * (game_manager->world_count - index - 1));

        free(game_manager->worlds);
        game_manager->worlds = new_worlds;

        game_manager->world_count -= 1;
    }
}

unsigned int
game_manager_add_tick_callback(
    te_game_manager* game_manager, void* custom,
    void (*on_tick)(void* custom, float delta_time_sec)) {
    te_game_tick_callback* new_callbacks =
        malloc(sizeof(te_game_tick_callback) * (game_manager->tick_callback_count + 1));
    memcpy(
        new_callbacks, game_manager->tick_callbacks,
        sizeof(te_game_tick_callback) * game_manager->tick_callback_count);

    free(game_manager->tick_callbacks);
    game_manager->tick_callbacks = new_callbacks;

    te_game_tick_callback* callback = &new_callbacks[game_manager->tick_callback_count];
    callback->on_tick = on_tick;
    callback->custom = custom;
    callback->id = game_manager->next_tick_callback_id;

    game_manager->next_tick_callback_id += 1;
    game_manager->tick_callback_count += 1;

    if (game_manager->is_processing_tick_callbacks) {
        game_manager->is_tick_callbacks_changed = true;
    }

    return callback->id;
}

void
game_manager_remove_tick_callback(te_game_manager* game_manager, unsigned int callback_id) {
    if (game_manager->tick_callback_count == 1) {
        free(game_manager->tick_callbacks);
        game_manager->tick_callbacks = NULL;
        game_manager->tick_callback_count = 0;
        return;
    }

    for (unsigned int i = 0; i < game_manager->tick_callback_count; i++) {
        if (game_manager->tick_callbacks[i].id != callback_id) {
            continue;
        }

        te_game_tick_callback* new_callbacks =
            malloc(sizeof(te_game_tick_callback) * (game_manager->tick_callback_count - 1));
        memcpy(new_callbacks, game_manager->tick_callbacks, sizeof(te_game_tick_callback) * i);
        memcpy(
            new_callbacks + i, game_manager->tick_callbacks + (i + 1),
            sizeof(te_game_tick_callback) * (game_manager->tick_callback_count - i - 1));

        free(game_manager->tick_callbacks);
        game_manager->tick_callbacks = new_callbacks;

        if (game_manager->is_processing_tick_callbacks) {
            game_manager->is_tick_callbacks_changed = true;
        }

        return;
    }

    log_error_fmt("unable to find a registered tick callback with ID %u", callback_id);
    abort();
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

te_sound_manager*
game_manager_get_sound_manager(te_game_manager* game_manager) {
    return game_manager->sound_manager;
}

unsigned int
game_manager_get_tick_callback_count(te_game_manager* game_manager) {
    return game_manager->tick_callback_count;
}

void
prv_game_manager_tick(te_game_manager* game_manager, float delta_time_sec) {
    (void)delta_time_sec;

    // Get active camera to update sound listener.
    te_camera* active_camera = NULL;
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        active_camera = world_get_active_camera(game_manager->worlds[i]);
        if (active_camera == NULL) {
            continue;
        }

#if defined(ENGINE_EDITOR)
        // Use game world camera as sound listener.
        if (strcmp(world_get_name(game_manager->worlds[i]), "game") == 0) {
            break;
        } else {
            active_camera = NULL;
        }
#else
        // Just use the first active camera.
        break;
#endif
    }

    if (active_camera != NULL) {
        vec3 pos;
        camera_get_world_position(active_camera, pos);

        vec3 forward;
        vec3 up;
        camera_get_forward(active_camera, forward);
        camera_get_up(active_camera, up);

        prv_sound_manager_set_listener(game_manager->sound_manager, pos, forward, up);
    }

    // Tick worlds.
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        prv_world_tick(game_manager->worlds[i]);
    }

    // Tick custom callbacks.
    {
        game_manager->is_processing_tick_callbacks = true;

        // Because logic inside a callback can add/remove tick callbacks (array we are iterating)
        // we track which callbacks we triggered and recheck if needed.
        unsigned int* notified_ids =
            malloc(sizeof(unsigned int) * game_manager->tick_callback_count);
        unsigned int notified_count = 0;

        while (true) {
            bool check_if_notified = false;

            if (game_manager->is_tick_callbacks_changed) {
                unsigned int* new_ids = malloc(
                    sizeof(unsigned int)
                    * (notified_count + game_manager->tick_callback_count));
                memcpy(new_ids, notified_ids, sizeof(unsigned int) * notified_count);

                free(notified_ids);
                notified_ids = new_ids;

                game_manager->is_tick_callbacks_changed = false;
                check_if_notified = true;
            }

            for (unsigned int callback_idx = 0;
                 callback_idx < game_manager->tick_callback_count; callback_idx++) {
                te_game_tick_callback* callback = &game_manager->tick_callbacks[callback_idx];

                if (check_if_notified) {
                    bool notified = false;
                    for (unsigned int i = 0; i < notified_count; i++) {
                        if (notified_ids[i] == callback->id) {
                            notified = true;
                            break;
                        }
                    }
                    if (notified) {
                        continue;
                    }
                }

                callback->on_tick(callback->custom, delta_time_sec);

                notified_ids[notified_count] = callback->id;
                notified_count += 1;

                if (game_manager->is_tick_callbacks_changed) {
                    break;
                }
            }

            if (game_manager->is_tick_callbacks_changed) {
                continue;
            }

            break;
        }

        free(notified_ids);

        game_manager->is_processing_tick_callbacks = false;
    }
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
prv_game_manager_on_mouse_button_pressed(
    te_game_manager* game_manager, enum te_mouse_button button) {
    vec2 cursor_pos;
    bool is_handled = false;
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        if (!world_get_cursor_relative_pos(game_manager->worlds[i], cursor_pos)) {
            continue;
        }
        is_handled |=
            prv_world_on_mouse_button_pressed(game_manager->worlds[i], button, cursor_pos);
    }

    return is_handled;
}

bool
prv_game_manager_on_mouse_button_released(
    te_game_manager* game_manager, enum te_mouse_button button) {
    vec2 cursor_pos;
    bool is_handled = false;
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        if (!world_get_cursor_relative_pos(game_manager->worlds[i], cursor_pos)) {
            continue;
        }
        is_handled |=
            prv_world_on_mouse_button_released(game_manager->worlds[i], button, cursor_pos);
    }

    return is_handled;
}

void
prv_game_manager_on_mouse_moved(te_game_manager* game_manager) {
    vec2 cursor_pos;
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        if (!world_get_cursor_relative_pos(game_manager->worlds[i], cursor_pos)) {
            continue;
        }
        prv_world_on_mouse_moved(game_manager->worlds[i], cursor_pos);
    }
}

void
prv_game_manager_on_mouse_cursor_captured(te_game_manager* game_manager, bool captured) {
    vec2 cursor_pos;
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        if (!world_get_cursor_relative_pos(game_manager->worlds[i], cursor_pos)) {
            continue;
        }
        prv_world_on_mouse_cursor_captured(game_manager->worlds[i], captured, cursor_pos);
    }
}

void
prv_game_manager_on_keyboard_input_text(te_game_manager* game_manager, const char* text) {
    for (unsigned int i = 0; i < game_manager->world_count; i++) {
        prv_world_on_keyboard_input_text(game_manager->worlds[i], text);
    }
}

void
prv_game_manager_on_keyboard_input(
    te_game_manager* game_manager, enum te_keyboard_button button, bool is_repeat) {
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
