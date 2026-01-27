#pragma once

typedef struct te_world te_world;

struct te_game_manager;
struct te_model_renderer;
struct te_camera;
struct te_model;

/**
 * Returns world's name.
 *
 * @return Do not free/destroy returned pointer.
 */
const char* world_get_name(te_world* world);

/**
 * Spawns the model in the world.
 *
 * @remark The model will be automatically despawned and destroyed when the world is being destroyed
 * but you can despawn the model earlier using @ref world_despawn_model.
 *
 * @param world World.
 * @param model Model to spawn.
 */
void world_spawn_model(te_world* world, struct te_model* model);

/**
 * Despawns a previously spawned model (see @ref world_spawn_model).
 *
 * @param world World.
 * @param model Model to despawn.
 */
void world_despawn_model(te_world* world, struct te_model* model);

/**
 * Spawns the camera in the world.
 *
 * @remark The camera will be automatically despawned and destroyed when the world is being destroyed
 * but you can despawn the camera earlier using @ref world_despawn_camera.
 *
 * @param world  World.
 * @param camera Camera to spawn.
 */
void world_spawn_camera(te_world* world, struct te_camera* camera);

/**
 * Despawns a previously spawned camera (see @ref world_spawn_camera).
 *
 * @param world  World.
 * @param camera Camera to despawn.
 */
void world_despawn_camera(te_world* world, struct te_camera* camera);

/**
 * Sets the camera to view the world.
 *
 * @remark The camera must be previously spawned in this world.
 *
 * @param world  World.
 * @param camera Camara to make active or NULL to have no active camera.
 */
void world_set_active_camera(te_world* world, struct te_camera* camera);

/**
 * Returns NULL if the world has no active camera, otherwise pointer to a valid camera.
 *
 * @param world World.
 *
 * @return Active camera. Do not free/destroy returned pointer.
 * The pointer is valid until the camera is not destroyed.
 */
struct te_camera* world_get_active_camera(te_world* world);

/**
 * Returns model renderer.
 *
 * @param world World.
 *
 * @return Always valid pointer to world's model renderer. Do not free/destroy returned pointer.
 * Valid while the world exists.
 */
struct te_model_renderer* world_get_model_renderer(te_world* world);

/**
 * Returns game manager.
 *
 * @param world World.
 *
 * @return Always valid pointer. Do not free/destroy the pointer.
 */
struct te_game_manager* world_get_game_manager(te_world* world);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

/**
 * Creates a new world. Game manager is expected to call this function because it manages game worlds.
 *
 * @param game_manager Game manager.
 * @param name         Non-NULL pointer to the world's name. The name will be copied to the world's object.
 */
te_world* prv_world_create(struct te_game_manager* game_manager, const char* name);

/**
 * Destroys the world. Game manager is expected to call this function because it manages game worlds.
 *
 * @param world World to destroy.
 */
void prv_world_destroy(te_world* world);
