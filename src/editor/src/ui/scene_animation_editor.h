#pragma once

#include <input/mouse_button.h>
#include <cglm/vec2.h>

typedef struct te_scene_animation_editor te_scene_animation_editor;
struct te_world;
struct te_widget;
struct te_type_info;

te_scene_animation_editor* scene_animation_editor_create(struct te_world* world);
void scene_animation_editor_destroy(te_scene_animation_editor* editor);

// Specify NULL as obj to hide.
void scene_animation_editor_show_tracks(
    te_scene_animation_editor* editor, void* obj, const struct te_type_info* type_info);

// Returns always valid widget, do not destroy returned pointer.
struct te_widget* scene_animation_editor_get_root_widget(te_scene_animation_editor* editor);

void scene_animation_editor_hide(te_scene_animation_editor* editor);
void scene_animation_editor_show(te_scene_animation_editor* editor);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Cursor pos in range [0.0; 1.0] relative to scene animation root widget pos/size.
void prv_scene_animation_editor_on_mouse_click(
    te_scene_animation_editor* editor, enum te_mouse_button button, vec2 cursor_pos);

void prv_scene_animation_editor_tick(te_scene_animation_editor* editor);
