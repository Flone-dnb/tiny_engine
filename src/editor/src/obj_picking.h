#pragma once

struct te_camera;
struct te_world;

#include "cglm/vec2.h"

// Looks for a 3D world game object that was under the cursor.
// Cursor position must be in range [0.0; 1.0] relative to the window size.
// Returns NULL if nothing found, otherwise pointer to a game object (for example: te_model).
void* obj_picking_find_obj_under_cursor(
    vec2 cursor_pos_rel, struct te_camera* camera, struct te_world* world);