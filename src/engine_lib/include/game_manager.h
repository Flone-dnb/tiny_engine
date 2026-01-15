#pragma once

typedef struct te_game_manager te_game_manager;

struct te_window;
struct te_world;

/**
 * Creates a new game manager.
 *
 * @param window Window that owns this object.
 * @param on_game_tick User callback that should be called on game tick.
 *
 * @return Created game manager.
 */
te_game_manager* game_manager_create(struct te_window* window,
                                     void (*on_game_tick)(te_game_manager* game_manager,
                                                          float delta_time_sec));

/**
 * Destroys game manager.
 *
 * @param game_manager Game manager to destroy.
 */
void game_manager_destroy(te_game_manager* game_manager);

/**
 * Creates a new world.
 *
 * @param game_manager Game manager.
 * @param name         World name. The name will be copied to the world's object.
 */
void game_manager_create_world(te_game_manager* game_manager, const char* name);

/**
 * Returns window that owns game manager.
 *
 * @param game_manager Game manager.
 *
 * @return Always valid pointer to the window. You should not free/destroy returned pointer.
 */
struct te_window* game_manager_get_window(te_game_manager* game_manager);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

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
