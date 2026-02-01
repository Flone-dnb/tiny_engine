#pragma once

typedef struct te_world te_world;

struct te_game_manager;
struct te_model_renderer;
struct te_camera;
struct te_model;

// Returns world's name.
// Do not free/destroy returned pointer.
const char* world_get_name(te_world* world);

// Spawns the model in the world.
//
// The model will be automatically despawned and destroyed when the world is being destroyed
// but you can despawn the model earlier using @ref world_despawn_model.
void world_spawn_model(te_world* world, struct te_model* model);
void world_despawn_model(te_world* world, struct te_model* model);

// Spawns the camera in the world.
//
// The camera will be automatically despawned and destroyed when the world is being destroyed
// but you can despawn the camera earlier using @ref world_despawn_camera.
void world_spawn_camera(te_world* world, struct te_camera* camera);
void world_despawn_camera(te_world* world, struct te_camera* camera);

// Sets the camera to view the world.
// Specify NULL to remove active camera.
//
// The camera must be previously spawned in this world.
void world_set_active_camera(te_world* world, struct te_camera* camera);

// Returns NULL if the world has no active camera.
// Do not free/destroy returned pointer, valid until the camera is not destroyed.
struct te_camera* world_get_active_camera(te_world* world);

// Returns model renderer.
// Do not free/destroy returned pointer, valid while the world exists.
struct te_model_renderer* world_get_model_renderer(te_world* world);

// Returns game manager.
// Always valid pointer. Do not free/destroy returned pointer.
struct te_game_manager* world_get_game_manager(te_world* world);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Creates a new world. Game manager is expected to call this function because it manages game worlds.
te_world* prv_world_create(struct te_game_manager* game_manager, const char* name);
void prv_world_destroy(te_world* world);
