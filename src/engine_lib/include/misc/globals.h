#pragma once

/**
 * Returns pointer to a static string that stores application name.
 *
 * @return Application name.
 */
const char* globals_get_app_name(void);

/**
 * Returns a normalized vector that points in the world's forward direction.
 *
 * @param out Value to write the result to. 4th component is zero.
 */
void globals_get_world_forward(float out[4]);

/**
 * Returns a normalized vector that points in the world's right direction.
 *
 * @param out Value to write the result to. 4th component is zero.
 */
void globals_get_world_right(float out[4]);

/**
 * Returns a normalized vector that points in the world's up direction.
 *
 * @param out Value to write the result to. 4th component is zero.
 */
void globals_get_world_up(float out[4]);
