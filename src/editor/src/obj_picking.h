#pragma once

struct te_camera;
struct te_world;
struct te_gizmo;

#include "cglm/vec2.h"

// Looks for a 3D world game object that was under the cursor.
// Cursor position must be in range [0.0; 1.0] relative to the window size.
// Gizmo may be NULL if not shown yet.
//
// Returns NULL if nothing found, otherwise pointer to a game object (for example: te_model)
// or a gizmo's model if gizmo was clicked.
void* obj_picking_find_obj_under_cursor(
    vec2 cursor_pos_rel, struct te_camera* camera, struct te_world* world, struct te_gizmo* gizmo);