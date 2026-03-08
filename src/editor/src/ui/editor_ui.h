#pragma once

typedef struct te_editor_ui te_editor_ui;
struct te_world;

te_editor_ui* editor_ui_create(void);
void editor_ui_destroy(te_editor_ui* ui);

// Creates and spawns editor's UI widgets.
void ui_spawn(te_editor_ui* ui, struct te_world* editor_world);
