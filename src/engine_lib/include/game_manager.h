#pragma once

struct te_window;
struct te_renderer;

/** Stores all core systems such as ECS, physics, audio, renderer and etc. */
typedef struct te_game_manager {
    /** Always valid pointer to the window that owns this object. This pointer should not be freed. */
    struct te_window* window;

    /** Renderer created by game manager. */
    struct te_renderer* renderer;

    /** User callback that should be called on game tick. */
    void (*on_game_tick)(struct te_game_manager* game_manager, float delta_time_sec);
} te_game_manager;

/**
 * Creates a new game manager.
 *
 * @param window Window that owns this object.
 * @param on_game_tick User callback that should be called on game tick.
 *
 * @return Created game manager.
 */
te_game_manager* game_manager_create(struct te_window* window,
                                     void (*on_game_tick)(te_game_manager* game_manager, float delta_time_sec));

/**
 * Destroys game manager.
 *
 * @param game_manager Game manager to destroy.
 */
void game_manager_destroy(te_game_manager* game_manager);

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
