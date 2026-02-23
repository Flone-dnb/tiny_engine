#pragma once

// Stacks child widgets vertically (AKA vertical layout widget).
typedef struct te_vbox_widget te_vbox_widget;
struct te_widget;

te_vbox_widget* vbox_widget_create(void);
void vbox_widget_destroy(te_vbox_widget* vbox_widget);

// Returns component used to adjust common widget properties (pos, size)
// and attach widget to other widgets.
// You can destroy returned object and it will cause this widget to be destroyed.
struct te_widget* vbox_widget_get_widget(te_vbox_widget* vbox_widget);

// Sets spacing between child widgets in range [0.0; 1.0] relative to window's height.
void vbox_widget_set_child_spacing(te_vbox_widget* vbox_widget, float spacing);
float vbox_widget_get_child_spacing(te_vbox_widget* vbox_widget);
