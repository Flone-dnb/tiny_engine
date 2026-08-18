#pragma once

#include <cglm/vec3.h>

enum te_game_object_type { TE_GOT_CAMERA, TE_GOT_MODEL, TE_GOT_PARTICLE_EMITTER };

struct te_world;

// Every game object (camera, model, etc.) has such an object.
// This object is valid while the actual game object is valid.
typedef struct te_game_object_info {
    // NULL if not registered in the reflected type database, otherwise a unique identifier of the type.
    // Points to a static string.
    const char* type_id;

    // Returns NULL if not spawned.
    struct te_world* (*get_world)(void* game_object);

    // Returns NULL if has no name, otherwise name of the game object (used for logging, debugging purposes).
    const char* (*get_name)(void* game_object);

    // NULL if not applicable.
    void (*get_position)(void* game_object, vec3 out);
    void (*set_position)(void* game_object, vec3 pos);

    // Called by world to notify game object.
    void (*on_spawned)(void* game_object, struct te_world* world);
    void (*on_despawned)(void* game_object);

    // Destroys the game object.
    void (*destroy)(void* game_object);

    enum te_game_object_type type;
} te_game_object_info;

typedef struct te_game_object_data {
    void* object;
    te_game_object_info* info;
} te_game_object_data;
