#pragma once

typedef struct te_scene_animation_editor te_scene_animation_editor;
struct te_world;
struct te_widget;

te_scene_animation_editor* scene_animation_editor_create(struct te_world* world);
void scene_animation_editor_destroy(te_scene_animation_editor* editor);

// Returns always valid widget, do not destroy returned pointer.
struct te_widget* scene_animation_editor_get_root_widget(te_scene_animation_editor* editor);

void scene_animation_editor_hide(te_scene_animation_editor* editor);
void scene_animation_editor_show(te_scene_animation_editor* editor);
