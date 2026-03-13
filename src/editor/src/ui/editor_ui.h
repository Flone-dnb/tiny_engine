#pragma once

typedef struct te_editor_ui te_editor_ui;
struct te_world;
struct te_world_inspector;

te_editor_ui* editor_ui_create(void);
void editor_ui_destroy(te_editor_ui* ui);

// Creates and spawns editor's UI widgets.
void editor_ui_spawn(te_editor_ui* ui, struct te_world* editor_world);

// Returns always valid pointer to world inspector.
// Do not delete/free returned pointer.
struct te_world_inspector* editor_ui_get_world_inspector(te_editor_ui* ui);
