#pragma once

typedef struct te_property_inspector te_property_inspector;
struct te_widget;
struct te_editor_ui;

te_property_inspector* property_inspector_create(struct te_editor_ui* ui);
void property_inspector_destroy(te_property_inspector* inspector);

void property_inspector_set_parent(te_property_inspector* inspector, struct te_widget* right_panel);

// Displays/hides properties of the specified object.
void property_inspector_show(te_property_inspector* inspector, void* obj, const char* obj_type_id);
void property_inspector_hide(te_property_inspector* inspector);

// Returns NULL or the currently displayed game object.
void* property_inspector_get_inspected_obj(te_property_inspector* inspector);