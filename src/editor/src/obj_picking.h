#pragma once

struct te_camera;
struct te_world;
struct te_gizmo;
struct te_game_object_info;

#include "cglm/vec2.h"

// Looks for a 3D world game object that was under the cursor.
// Cursor position must be in range [0.0; 1.0] relative to the window size.
// Gizmo may be NULL if not shown yet.
void obj_picking_find_obj_under_cursor(
    vec2 cursor_pos_rel, struct te_camera* camera, struct te_world* world,
    struct te_gizmo* gizmo, void** out_game_obj,
    struct te_game_object_info** out_game_obj_info);
