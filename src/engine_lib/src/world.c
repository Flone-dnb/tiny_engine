#include "world.h"

#include <stdlib.h>
#include <string.h>

#include "game/camera.h"
#include "game/model.h"
#include "game_manager.h"
#include "misc/error.h"
#include "render/model_renderer.h"
#include "render/widget_renderer.h"
#include "widget/widget.h"
#include "window.h"
#if defined(ENGINE_DEBUG_TOOLS)
#include "glad/glad.h"
#include "render/gpu_time_section.h"
#endif

// World represents several objects: audio system, cameras, game objects and etc.
struct te_world {
    // Always valid pointer. Game manager that owns this world. You should not free/destroy this pointer.
    struct te_game_manager* game_manager;

    // NULL if no active camera. Do not free/destroy this pointer. The camera will register/unregister itself.
    te_camera* active_camera;

    // Always valid pointer, size of this array is @ref spawned_models_array_size but the actually
    // used number of elements is @ref spawned_model_count. If some model despawned some pointers
    // will be shifted to keep the array valid without any "holes". This array does not shrink
    // but the number of used (valid) elements may decrease.
    te_model** spawned_models;

    // NULL if nothing spawned, size of this array is @ref spawned_camera_count.
    te_camera** spawned_cameras;

    // NULL if nothing spawned, size of this array is @ref spawned_widget_count.
    // Each widget here can have child widgets, this array only stores root widgets.
    te_widget** spawned_widgets;

    // Spawned widgets (from @ref spawned_widgets) that receive input (for example buttons).
    // Size of this array is @ref interactable_widget_count.
    te_widget** interactable_widgets;

    // Renders models of the world.
    te_model_renderer* opaque_model_renderer;
    te_model_renderer* transparent_model_renderer;

    // Renders widgets of the world.
    te_widget_renderer* widget_renderer;

    // May be NULL. Item from array @ref interactable_widgets that currently hovered.
    te_widget* hovered_interactable_widget;

    // World name.
    char* name;

    // Number of spawned models (valid elements) in @ref spawned_models.
    unsigned int spawned_model_count;

    // Total number of elements that @ref spawned_models can hold.
    unsigned int spawned_models_array_size;

    // Size of the array @ref spawned_cameras.
    unsigned int spawned_camera_count;

    // Size of the array @ref spawned_widgets.
    unsigned int spawned_widget_count;

    // Size of the array @ref interactable_widgets.
    unsigned int interactable_widget_count;

    // `true` if the world is currently being destroyed.
    bool is_being_destroyed;

#if defined(ENGINE_DEBUG_TOOLS)
    // GPU time query IDs.
    unsigned int gl_query_draw_models;
    unsigned int gl_query_draw_widgets;
#endif
};

te_world*
prv_world_create(struct te_game_manager* game_manager, const char* name) {
    if (name == NULL) {
        show_error_and_abort("world name must not be NULL");
    }

    te_world* world = malloc(sizeof(te_world));

    world->game_manager = game_manager;

    world->active_camera = NULL;

    world->spawned_cameras = NULL;
    world->spawned_camera_count = 0;

    world->spawned_widgets = NULL;
    world->spawned_widget_count = 0;

    world->interactable_widgets = NULL;
    world->hovered_interactable_widget = NULL;
    world->interactable_widget_count = 0;

    world->spawned_model_count = 0;
    world->spawned_models_array_size = 128;
    world->spawned_models = malloc(sizeof(te_model*) * world->spawned_models_array_size);

    world->opaque_model_renderer = model_renderer_create(128, 128);
    world->transparent_model_renderer = model_renderer_create(4, 4);
    world->widget_renderer = widget_renderer_create(game_manager_get_renderer(game_manager));
    world->is_being_destroyed = false;

    // Copy name.
    const size_t name_len = strlen(name);
    world->name = malloc(sizeof(char) * (name_len + 1));
    memcpy(world->name, name, name_len);
    world->name[name_len] = 0;

#if defined(ENGINE_DEBUG_TOOLS)
    if (GLAD_GL_EXT_disjoint_timer_query == 1) {
        glGenQueriesEXT(1, &world->gl_query_draw_models);
        glGenQueriesEXT(1, &world->gl_query_draw_widgets);

        // Init timers.
        GPU_TIME_SECTION_BEGIN(world->gl_query_draw_models);
        GPU_TIME_SECTION_END;
        GPU_TIME_SECTION_BEGIN(world->gl_query_draw_widgets);
        GPU_TIME_SECTION_END;
    }
#endif

    return world;
}

void
prv_world_destroy(te_world* world) {
    world->is_being_destroyed = true;

    // Despawn and destroy world objects.
    {
        // Models.
        while (world->spawned_model_count > 0) {
            te_model* model = world->spawned_models[world->spawned_model_count - 1];
            world->spawned_model_count -= 1;
            prv_model_on_despawned(model);
            model_destroy(model);
        }
        free(world->spawned_models);

        // Cameras.
        while (world->spawned_camera_count > 0) {
            te_camera* camera = world->spawned_cameras[world->spawned_camera_count - 1];
            world->spawned_camera_count -= 1;
            camera_destroy(camera);
        }
        free(world->spawned_cameras);

        // Widgets.
        while (world->spawned_widget_count > 0) {
            te_widget* widget = world->spawned_widgets[world->spawned_widget_count - 1];
            world->spawned_widget_count -= 1;
            prv_widget_on_despawned(widget);
            widget_destroy(widget);
        }
        free(world->spawned_widgets);
        if (world->interactable_widget_count > 0) {
            show_error_and_abort(
                "all widgets of a world were destroyed but there are still some interactable widgets registered");
        }
        free(world->interactable_widgets);
    }

    free(world->name);
    model_renderer_destroy(world->opaque_model_renderer);
    model_renderer_destroy(world->transparent_model_renderer);
    widget_renderer_destroy(world->widget_renderer);

#if defined(ENGINE_DEBUG_TOOLS)
    if (GLAD_GL_EXT_disjoint_timer_query == 1) {
        glDeleteQueriesEXT(1, &world->gl_query_draw_models);
        glDeleteQueriesEXT(1, &world->gl_query_draw_widgets);
    }
#endif

    free(world);
}

bool
prv_world_is_being_destroyed(te_world* world) {
    return world->is_being_destroyed;
}

void
prv_world_on_window_size_changed(te_world* world) {
    if (world->active_camera == NULL) {
        return;
    }

    for (unsigned int i = 0; i < world->spawned_widget_count; i++) {
        prv_widget_on_window_size_changed(world->spawned_widgets[i]);
    }
}

const char*
world_get_name(te_world* world) {
    return world->name;
}

void
world_set_active_camera(te_world* world, te_camera* camera) {
    if (world->active_camera == camera) {
        return;
    }

    if (camera_get_world(camera) != world) {
        show_error_and_abort("in order to make a camera active in the world you first need to spawn the camera in the world");
    }

    world->active_camera = camera;
}

te_camera*
world_get_active_camera(te_world* world) {
    return world->active_camera;
}

te_model_renderer*
world_get_opaque_model_renderer(te_world* world) {
    return world->opaque_model_renderer;
}

struct te_model_renderer*
world_get_transparent_model_renderer(te_world* world) {
    return world->transparent_model_renderer;
}

te_widget_renderer*
world_get_widget_renderer(te_world* world) {
    return world->widget_renderer;
}

struct te_game_manager*
world_get_game_manager(te_world* world) {
    return world->game_manager;
}

void
world_spawn_model(te_world* world, te_model* model) {
    if (world->is_being_destroyed) {
        return;
    }

    te_world* old_model_world = model_get_world(model);
    if (old_model_world != NULL) {
        if (old_model_world == world) {
            show_error_and_abort("the model is already spawned in this world");
        } else {
            show_error_and_abort("the specified model cannot be spawned in this world because the model "
                                 "must be first despawned from the world it currently resides in");
        }
    }

    if (world->spawned_model_count == world->spawned_models_array_size) {
        // Expand array.
        const unsigned int grow_size = 128;
        te_model** new_models = malloc(sizeof(te_model*) * (world->spawned_models_array_size + grow_size));
        memcpy(new_models, world->spawned_models, sizeof(te_model*) * world->spawned_model_count);

        free(world->spawned_models);
        world->spawned_models = new_models;
        world->spawned_models_array_size += grow_size;
    }

    world->spawned_models[world->spawned_model_count] = model;
    world->spawned_model_count += 1;

    // Notify model as the last step as it can spawn a child model here.
    prv_model_on_spawned(model, world);
}

void
world_despawn_model(te_world* world, te_model* model) {
    if (model_get_world(model) != world) {
        show_error_and_abort("the specified model cannot be despawned from this world as it's not spawned in this world");
    }

    // Find model.
    unsigned int model_idx = 0;
    bool found = false;
    for (unsigned int i = 0; i < world->spawned_model_count; i++) {
        if (world->spawned_models[i] != model) {
            continue;
        }

        model_idx = i;
        found = true;
    }
    if (!found) {
        show_error_and_abort("the specified model (to despawn) was not spawned previously");
    }

    // Remove from array (shift other elements).
    memcpy(
        world->spawned_models + model_idx, world->spawned_models + (model_idx + 1),
        sizeof(te_model*) * (world->spawned_model_count - model_idx - 1));
    world->spawned_model_count -= 1;

    // Notify model as the last step as it can cause a child model to be despawned here.
    prv_model_on_despawned(model);
}

void
world_spawn_camera(te_world* world, te_camera* camera) {
    if (world->is_being_destroyed) {
        return;
    }

#if defined(DEBUG)
    if (camera == NULL) {
        show_error_and_abort("NULL camera specified to spawn");
    }
#endif

    te_world* old_camera_world = camera_get_world(camera);
    if (old_camera_world != NULL) {
        if (old_camera_world == world) {
            show_error_and_abort("the camera is already spawned in this world");
        } else {
            show_error_and_abort("the specified camera cannot be spawned in this world because the camera "
                                 "must be first despawned from the world it currently resides in");
        }
    }

    te_camera** new_cameras = malloc(sizeof(te_camera*) * (world->spawned_camera_count + 1));
    memcpy(new_cameras, world->spawned_cameras, sizeof(te_camera*) * world->spawned_camera_count);

    free(world->spawned_cameras);
    world->spawned_cameras = new_cameras;

    new_cameras[world->spawned_camera_count] = camera;
    world->spawned_camera_count += 1;

    prv_camera_set_world(camera, world);
}

void
world_despawn_camera(te_world* world, te_camera* camera) {
#if defined(DEBUG)
    if (camera == NULL) {
        show_error_and_abort("NULL camera specified to despawn");
    }
#endif

    if (camera_get_world(camera) != world) {
        show_error_and_abort("the specified camera cannot be despawned from this world as it's not spawned in this world");
    }

    if (world->active_camera == camera) {
        world->active_camera = NULL;
    }
    prv_camera_set_world(camera, NULL);

    if (world->spawned_camera_count == 1) {
        free(world->spawned_cameras);
        world->spawned_cameras = NULL;
        world->spawned_camera_count = 0;
    } else {
        unsigned int i = 0;
        bool found = false;
        for (; i < world->spawned_camera_count; i++) {
            if (world->spawned_cameras[i] != camera) {
                continue;
            }

            found = true;
            break;
        }
        if (!found) {
            show_error_and_abort("unable to find the specified camera");
        }

        te_camera** new_cameras = malloc(sizeof(te_camera*) * (world->spawned_model_count - 1));
        memcpy(new_cameras, world->spawned_cameras, sizeof(te_camera*) * i);
        memcpy(new_cameras + i, world->spawned_cameras + (i + 1), sizeof(te_camera*) * (world->spawned_camera_count - i - 1));

        free(world->spawned_cameras);
        world->spawned_cameras = new_cameras;
        world->spawned_camera_count -= 1;
    }
}

void
world_spawn_widget(te_world* world, struct te_widget* widget) {
    if (world->is_being_destroyed) {
        return;
    }

    te_world* old_widget_world = widget_get_world(widget);
    if (old_widget_world != NULL) {
        if (old_widget_world == world) {
            show_error_and_abort("the widget is already spawned in this world");
        } else {
            show_error_and_abort("the specified widget cannot be spawned in this world because the widget "
                                 "must be first despawned from the world it currently resides in");
        }
    }

    te_widget** new_widgets = malloc(sizeof(te_widget*) * (world->spawned_widget_count + 1));
    memcpy(new_widgets, world->spawned_widgets, sizeof(te_widget*) * world->spawned_widget_count);

    free(world->spawned_widgets);
    world->spawned_widgets = new_widgets;

    world->spawned_widgets[world->spawned_widget_count] = widget;
    world->spawned_widget_count += 1;

    prv_widget_on_spawned(widget, world);
}

void
world_despawn_widget(te_world* world, te_widget* widget) {
    if (widget_get_world(widget) != world) {
        show_error_and_abort("the specified widget cannot be despawned from this world as it's not spawned in this world");
    }

    if (world->spawned_widget_count == 1) {
        free(world->spawned_widgets);
        world->spawned_widgets = NULL;
        world->spawned_widget_count = 0;
    } else {
        unsigned int i = 0;
        bool found = false;
        for (; i < world->spawned_widget_count; i++) {
            if (world->spawned_widgets[i] != widget) {
                continue;
            }

            found = true;
            break;
        }
        if (!found) {
            show_error_and_abort("unable to find the specified widget");
        }

        te_widget** new_widgets = malloc(sizeof(te_widget*) * (world->spawned_widget_count - 1));
        memcpy(new_widgets, world->spawned_widgets, sizeof(te_widget*) * i);
        memcpy(new_widgets + i, world->spawned_widgets + (i + 1), sizeof(te_widget*) * (world->spawned_widget_count - i - 1));

        free(world->spawned_widgets);
        world->spawned_widgets = new_widgets;
        world->spawned_widget_count -= 1;
    }

    prv_widget_on_despawned(widget);
}

void
prv_world_add_interactable_widget(te_world* world, te_widget* widget) {
    te_widget** new_widgets = malloc(sizeof(te_widget*) * (world->interactable_widget_count + 1));
    memcpy(new_widgets, world->interactable_widgets, sizeof(te_widget*) * world->interactable_widget_count);

    free(world->interactable_widgets);
    world->interactable_widgets = new_widgets;

    world->interactable_widgets[world->interactable_widget_count] = widget;
    world->interactable_widget_count += 1;
}

void
prv_world_remove_interactable_widget(te_world* world, te_widget* widget) {
    if (widget == world->hovered_interactable_widget) {
        world->hovered_interactable_widget = NULL;
    }

    if (world->interactable_widget_count == 1) {
        free(world->interactable_widgets);
        world->interactable_widgets = NULL;
        world->interactable_widget_count = 0;
    } else {
        unsigned int i = 0;
        bool found = false;
        for (; i < world->interactable_widget_count; i++) {
            if (world->interactable_widgets[i] != widget) {
                continue;
            }

            found = true;
            break;
        }
        if (!found) {
            show_error_and_abort("unable to find the specified widget");
        }

        te_widget** new_widgets = malloc(sizeof(te_widget*) * (world->interactable_widget_count - 1));
        memcpy(new_widgets, world->interactable_widgets, sizeof(te_widget*) * i);
        memcpy(
            new_widgets + i, world->interactable_widgets + (i + 1),
            sizeof(te_widget*) * (world->interactable_widget_count - i - 1));

        free(world->interactable_widgets);
        world->interactable_widgets = new_widgets;
        world->interactable_widget_count -= 1;
    }
}

void
prv_world_interactable_widget_pos_size_changed(te_world* world) {
    te_window* window = game_manager_get_window(world->game_manager);

    vec2 cursor_pos;
    window_get_cursor_position(window, &cursor_pos[0], &cursor_pos[1]);

    unsigned int window_width;
    unsigned int window_height;
    window_get_size(window, &window_width, &window_height);

    glm_vec2_div(cursor_pos, (vec2){(float)window_width, (float)window_height}, cursor_pos);

    prv_world_on_mouse_moved(world, cursor_pos);
}

void
prv_world_on_mouse_moved(te_world* world, float cursor_pos[2]) {
    vec2 pos;
    vec2 size;
    for (unsigned int i = 0; i < world->interactable_widget_count; i++) {
        widget_get_screen_position(world->interactable_widgets[i], pos);
        if (pos[0] > cursor_pos[0] || pos[1] > cursor_pos[1]) {
            continue;
        }

        widget_get_screen_size(world->interactable_widgets[i], size);
        if (cursor_pos[0] > pos[0] + size[0] || cursor_pos[1] > pos[1] + size[1]) {
            continue;
        }

        if (world->hovered_interactable_widget == NULL) {
            world->hovered_interactable_widget = world->interactable_widgets[i];
            prv_widget_on_cursor_entered(world->hovered_interactable_widget, cursor_pos);
        } else {
            if (world->hovered_interactable_widget != world->interactable_widgets[i]) {
                prv_widget_on_cursor_left(world->hovered_interactable_widget, cursor_pos);

                world->hovered_interactable_widget = world->interactable_widgets[i];
                prv_widget_on_cursor_entered(world->hovered_interactable_widget, cursor_pos);
            } else {
                prv_widget_on_hovered_cursor_moved(world->hovered_interactable_widget, cursor_pos);
            }
        }

        return;
    }

    if (world->hovered_interactable_widget != NULL) {
        prv_widget_on_cursor_left(world->hovered_interactable_widget, cursor_pos);
        world->hovered_interactable_widget = NULL;
    }
}

bool
prv_world_on_mouse_button_pressed(te_world* world, enum te_mouse_button button, float cursor_pos[2]) {
    vec2 pos;
    vec2 size;
    for (unsigned int i = 0; i < world->interactable_widget_count; i++) {
        widget_get_screen_position(world->interactable_widgets[i], pos);
        if (pos[0] > cursor_pos[0] || pos[1] > cursor_pos[1]) {
            continue;
        }

        widget_get_screen_size(world->interactable_widgets[i], size);
        if (cursor_pos[0] > pos[0] + size[0] || cursor_pos[1] > pos[1] + size[1]) {
            continue;
        }

        prv_widget_on_mouse_button_pressed(world->interactable_widgets[i], button, cursor_pos);
        return true;
    }

    return false;
}

bool
prv_world_on_mouse_button_released(te_world* world, enum te_mouse_button button, float cursor_pos[2]) {
    vec2 pos;
    vec2 size;
    for (unsigned int i = 0; i < world->interactable_widget_count; i++) {
        widget_get_screen_position(world->interactable_widgets[i], pos);
        if (pos[0] > cursor_pos[0] || pos[1] > cursor_pos[1]) {
            continue;
        }

        widget_get_screen_size(world->interactable_widgets[i], size);
        if (cursor_pos[0] > pos[0] + size[0] || cursor_pos[1] > pos[1] + size[1]) {
            continue;
        }

        prv_widget_on_mouse_button_released(world->interactable_widgets[i], button, cursor_pos);
        return true;
    }

    return false;
}

void
prv_world_on_keyboard_input_text(te_world* world, const char* text) {
    if (world->hovered_interactable_widget == NULL) {
        return;
    }

    prv_widget_on_keyboard_input_text(world->hovered_interactable_widget, text);
}

void
prv_world_on_keyboard_input(te_world* world, enum te_keyboard_button button, bool is_repeat) {
    // Don't care if repeat or not for UI.
    (void)is_repeat;

    if (world->hovered_interactable_widget == NULL) {
        return;
    }

    prv_widget_on_keyboard_input(world->hovered_interactable_widget, button);
}

void
prv_world_on_input_source_changed(te_world* world) {
    if (world->hovered_interactable_widget != NULL) {
        te_window* window = game_manager_get_window(world->game_manager);

        vec2 cursor_pos;
        window_get_cursor_position(window, &cursor_pos[0], &cursor_pos[1]);

        unsigned int window_width;
        unsigned int window_height;
        window_get_size(window, &window_width, &window_height);

        glm_vec2_div(cursor_pos, (vec2){(float)window_width, (float)window_height}, cursor_pos);

        prv_widget_on_cursor_left(world->hovered_interactable_widget, cursor_pos);
        world->hovered_interactable_widget = NULL;
    }
}

bool
prv_world_find_root_widget(te_world* world, te_widget* widget) {
    for (unsigned int i = 0; i < world->spawned_widget_count; i++) {
        if (world->spawned_widgets[i] == widget) {
            return true;
        }
    }

    return false;
}

#if defined(ENGINE_DEBUG_TOOLS)
unsigned int
prv_world_get_gl_query_draw_models(te_world* world) {
    return world->gl_query_draw_models;
}

unsigned int
prv_world_get_gl_query_draw_widgets(te_world* world) {
    return world->gl_query_draw_widgets;
}
#endif
