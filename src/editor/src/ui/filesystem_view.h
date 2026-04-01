#pragma once

// Filesystem widget in the bottom-left corner of the editor's UI.
typedef struct te_filesystem_view te_filesystem_view;
struct te_widget;
struct te_editor;

te_filesystem_view* filesystem_view_create(struct te_editor* editor);
void filesystem_view_destroy(te_filesystem_view* explorer);

void filesystem_view_add(te_filesystem_view* explorer, struct te_widget* left_panel);

// Refreshes displayed directory entries.
void filesystem_view_refresh(te_filesystem_view* explorer);
