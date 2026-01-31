#include "world.h"

#include <stdlib.h>
#include <string.h>

#include "game/camera.h"
#include "game/model.h"
#include "misc/error.h"
#include "render/model_renderer.h"

/** World represents several objects: audio system, cameras, game objects and etc. */
struct te_world {
    /** Always valid pointer. Game manager that owns this world. You should not free/destroy this pointer. */
    struct te_game_manager* game_manager;

    /** NULL if no active camera. Do not free/destroy this pointer. The camera will register/unregister itself. */
    te_camera* active_camera;

    /**
     * Always valid pointer, size of this array is @ref spawned_models_array_size but the actually
     * used number of elements is @ref spawned_model_count. If some model despawned some pointers
     * will be shifted to keep the array valid without any "holes". This array does not shrink
     * but the number of used (valid) elements may decrease.
     */
    te_model** spawned_models;

    /** NULL if nothing spawned, size of this array is @ref spawned_camera_count. */
    te_camera** spawned_cameras;

    /** Renders models of the world. */
    te_model_renderer* model_renderer;

    /** World name. */
    char* name;

    /** Number of spawned models in @ref spawned_models. */
    unsigned int spawned_model_count;

    /** Size of the array @ref spawned_models. */
    unsigned int spawned_models_array_size;

    /** Size of the array @ref spawned_cameras. */
    unsigned int spawned_camera_count;

    /** `true` if the world is currently being destroyed. */
    bool is_being_destroyed;
};

te_world*
prv_world_create(struct te_game_manager* game_manager, const char* name) {
    if (name == NULL) {
        show_error_and_abort("world name should not be NULL");
    }

    te_world* world = malloc(sizeof(te_world));

    world->game_manager = game_manager;

    world->active_camera = NULL;
    world->spawned_cameras = NULL;
    world->spawned_camera_count = 0;

    world->spawned_model_count = 0;
    world->spawned_models_array_size = 128;
    world->spawned_models = malloc(sizeof(te_model*) * world->spawned_models_array_size);

    world->model_renderer = model_renderer_create();
    world->is_being_destroyed = false;

    // Copy name.
    const size_t name_len = strlen(name);
    world->name = malloc(sizeof(char) * (name_len + 1));
    memcpy(world->name, name, name_len);
    world->name[name_len] = 0;

    return world;
}

void
prv_world_destroy(te_world* world) {
    world->is_being_destroyed = true;

    // Despawn and destroy world objects.
    {
        while (world->spawned_model_count > 0) {
            te_model* model = world->spawned_models[world->spawned_model_count - 1];
            world->spawned_model_count -= 1;
            prv_model_on_despawned(model);
            model_destroy(model);
        }
        free(world->spawned_models);

        while (world->spawned_camera_count > 0) {
            te_camera* camera = world->spawned_cameras[world->spawned_camera_count - 1];
            world->spawned_camera_count -= 1;
            camera_destroy(camera);
        }
        free(world->spawned_cameras);
    }

    free(world->name);
    model_renderer_destroy(world->model_renderer);

    free(world);
}

const char*
world_get_name(te_world* world) {
    return world->name;
}

void
world_set_active_camera(te_world* world, te_camera* camera) {
    if (camera_get_world(camera) != world) {
        show_error_and_abort(
            "in order to make a camera active in the world you first need to spawn the camera in the world");
    }

    world->active_camera = camera;
}

te_camera*
world_get_active_camera(te_world* world) {
    return world->active_camera;
}

te_model_renderer*
world_get_model_renderer(te_world* world) {
    return world->model_renderer;
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

    prv_model_on_spawned(model, world);
}

void
world_despawn_model(te_world* world, te_model* model) {
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
    memcpy(world->spawned_models + model_idx, world->spawned_models + (model_idx + 1),
           sizeof(te_model*) * (world->spawned_model_count - model_idx - 1));
    world->spawned_model_count -= 1;

    prv_model_on_despawned(model);
}

void
world_spawn_camera(te_world* world, te_camera* camera) {
    if (camera_get_world(camera) != NULL) {
        show_error_and_abort("the specified camera cannot be spawned in this world because the camera "
                             "must be first despawned from the world it currently resides in");
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
    if (camera_get_world(camera) != world) {
        show_error_and_abort(
            "the specified camera cannot be despawned from this world as it's not spawned in this world");
    }

    prv_camera_set_world(camera, NULL);

    if (world->active_camera == camera) {
        world->active_camera = NULL;
    }

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
        memcpy(new_cameras + i, world->spawned_cameras + (i + 1), world->spawned_camera_count - i - 1);

        free(world->spawned_cameras);
        world->spawned_cameras = new_cameras;
        world->spawned_camera_count -= 1;
    }
}
