#pragma once

/* stacks child widgets vertically (AKA vertical layout widget) */
typedef struct te_vbox_widget te_vbox_widget;
struct te_widget;

te_vbox_widget* vbox_widget_create(void);
void vbox_widget_destroy(te_vbox_widget* vbox_widget);

/* returns component used to adjust common widget properties (pos, size)
 * and attach widget to other widgets
 * you can destroy returned object and it will cause this widget to be destroyed */
struct te_widget* vbox_widget_get_widget(te_vbox_widget* vbox_widget);

/* sets spacing between child widgets in range [0.0; 1.0] relative to window's height */
void vbox_widget_set_child_spacing(te_vbox_widget* vbox_widget, float spacing);
float vbox_widget_get_child_spacing(te_vbox_widget* vbox_widget);

/* returns unique ID of this type in the type database */
const char* vbox_widget_get_type_id(void);
/* registers the type in the type database */
void vbox_widget_register_type(void);
