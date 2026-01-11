#pragma once

struct te_game_manager;

/** World represents several objects: audio system, cameras, game objects and etc. */
typedef struct te_world {
    /** Always valid pointer. Game manager that owns this world. You should not free/destroy this pointer. */
    struct te_game_manager* game_manager;

    /** World name. */
    char* name;
} te_world;

/**
 * Returns world's name.
 *
 * @return Do not free/destroy returned pointer.
 */
const char* world_get_name(te_world* world);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

/**
 * Creates a new world. Game manager is expected to call this function because it manages game worlds.
 *
 * @param game_manager Game manager.
 * @param name         World name. The name will be copied to the world's object.
 */
te_world* prv_world_create(struct te_game_manager* game_manager, const char* name);

/**
 * Destroys the world. Game manager is expected to call this function because it manages game worlds.
 *
 * @param world World to destroy.
 */
void prv_world_destroy(te_world* world);
