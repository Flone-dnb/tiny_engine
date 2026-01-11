#pragma once

struct te_game_manager;

/**
 * Called when the editor can start.
 *
 * @param game_manager Game manager.
 */
void editor_on_game_started(struct te_game_manager* game_manager);

/**
 * Called before a new frame is rendered.
 *
 * @param game_manager Game manager.
 * @param delta_time_ms Time (in seconds) since the previous call to this function.
 */
void editor_on_game_tick(struct te_game_manager* game_manager, float delta_time_sec);

/**
 * Called before the window (editor) is closed.
 *
 * @param game_manager Game manager.
 */
void editor_on_window_close(struct te_game_manager* game_manager);
