#pragma once

typedef struct te_game_manager te_game_manager;

struct te_renderer;
struct te_window;
struct te_world;

/**
 * Creates a new world.
 *
 * @param game_manager Game manager.
 * @param name         World name. The name will be copied to the world's object.
 *
 * @return Created world.
 */
struct te_world* game_manager_create_world(te_game_manager* game_manager, const char* name);

/**
 * Destroys a world previously created using @ref game_manager_create_world.
 *
 * @param game_manager Game manager.
 * @param world        World to destroy.
 */
void game_manager_destroy_world(te_game_manager* game_manager, struct te_world* world);

/**
 * Returns window that owns game manager.
 *
 * @param game_manager Game manager.
 *
 * @return Always valid pointer to the window. You should not free/destroy returned pointer.
 */
struct te_window* game_manager_get_window(te_game_manager* game_manager);

/**
 * Returns renderer.
 *
 * @param game_manager Game manager.
 *
 * @return Always valid pointer to the renderer. You should not free/destroy returned pointer.
 */
struct te_renderer* game_manager_get_renderer(te_game_manager* game_manager);

/**
 * Returns all currently existing worlds.
 *
 * @param game_manager Game manager.
 * @param world_count  Specify a non-NULL pointer that will be filled with the number of elements in the returned array.
 *
 * @return NULL if no world exists, otherwise array of world pointers. Do not free/destroy returned pointer.
 */
struct te_world** game_manager_get_worlds(te_game_manager* game_manager, unsigned int* world_count);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

/**
 * Creates a new game manager.
 *
 * @param window Window that owns this object.
 *
 * @return Created game manager.
 */
te_game_manager* prv_game_manager_create(struct te_window* window);

/**
 * Destroys game manager.
 *
 * @param game_manager Game manager to destroy.
 */
void prv_game_manager_destroy(te_game_manager* game_manager);

/**
 * Called by window before a new frame is rendered.
 *
 * @param game_manager   Game manager.
 * @param delta_time_sec Time (in seconds) since the previous call to this function.
 */
void prv_game_manager_tick(te_game_manager* game_manager, float delta_time_sec);

/**
 * Called by window to render a new frame.
 *
 * @param game_manager Game manager.
 */
void prv_game_manager_draw_frame(te_game_manager* game_manager);

/**
 * Called by window after its size was changed.
 *
 * @param game_manager Game manager.
 */
void prv_game_manager_on_window_size_changed(te_game_manager* game_manager);
