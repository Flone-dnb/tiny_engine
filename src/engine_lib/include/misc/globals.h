#pragma once

// Returns application name.
// Do not free/destroy returned pointer.
const char* globals_get_app_name(void);

// Returns a normalized vector that points in the world's forward direction.
void globals_get_world_forward(float out[3]);

// Returns a normalized vector that points in the world's right direction.
void globals_get_world_right(float out[3]);

// Returns a normalized vector that points in the world's up direction.
void globals_get_world_up(float out[3]);
