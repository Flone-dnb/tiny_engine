#pragma once

#include <stdbool.h>

typedef struct te_editor_ui te_editor_ui;
struct te_editor;
struct te_world;
struct te_world_inspector;

te_editor_ui* editor_ui_create(struct te_editor* editor);
void editor_ui_destroy(te_editor_ui* ui);

// Creates and spawns editor's UI widgets.
void editor_ui_spawn(te_editor_ui* ui, struct te_world* editor_world);

// Refreshes displayed directory entries in file explorer.
void editor_ui_refresh_filesystem_view(te_editor_ui* ui);

void editor_ui_set_visibility(te_editor_ui* ui, bool is_visible);

// Clears all information in UI widgets (resets to their initial state).
void editor_ui_reset(te_editor_ui* ui);

// Returns always valid pointer to world inspector.
// Do not delete/free returned pointer.
struct te_world_inspector* editor_ui_get_world_inspector(te_editor_ui* ui);