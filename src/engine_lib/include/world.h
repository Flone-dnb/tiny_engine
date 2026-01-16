#pragma once

typedef struct te_world te_world;

struct te_game_manager;
struct te_ecs;

/**
 * Returns world's name.
 *
 * @return Do not free/destroy returned pointer.
 */
const char* world_get_name(te_world* world);

/**
 * Returns ECS manager of the world.
 *
 * @param world World.
 *
 * @return ECS manager.
 */
struct te_ecs* world_get_ecs(te_world* world);

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

/**
 * Called before a new frame is rendered.
 *
 * @param world World.
 * @param delta_time_sec Time (in seconds) since the previous call to this function.
 */
void prv_world_tick(te_world* world, float delta_time_sec);
