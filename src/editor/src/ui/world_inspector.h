#pragma once

typedef struct te_world_inspector te_world_inspector;
struct te_widget;
struct te_world;
struct te_editor;
struct te_property_inspector;
struct te_game_object_info;

te_world_inspector* world_inspector_create(struct te_editor* editor, struct te_property_inspector* property_inspector);
void world_inspector_destroy(te_world_inspector* inspector);

void world_inspector_add(te_world_inspector* inspector, struct te_widget* left_panel);
void world_inspector_rebuild_list(te_world_inspector* inspector, struct te_world* game_world);

// Looks for the specified game object and selects it if it exists in the world inspector.
// Specify NULL to clear selection.
void world_inspector_select_obj(te_world_inspector* inspector, struct te_game_object_info* target_info);

// In case some game object's name was changed call this function to make sure world inspector displays the updated name.
void world_inspector_refresh_names(te_world_inspector* inspector);
