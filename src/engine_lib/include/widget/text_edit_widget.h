#pragma once

#include <wchar.h>
#include "cglm/vec4.h"

typedef struct te_text_edit_widget te_text_edit_widget;
struct te_widget;
struct te_text_widget;

te_text_edit_widget* text_edit_widget_create(void (*on_text_changed)(wchar_t* new_text, unsigned int strlen));
void text_edit_widget_destroy(te_text_edit_widget* text_edit_widget);

// Returns component used to adjust common widget properties (pos, size)
// and attach widget to other widgets.
// You can destroy returned object and it will cause this widget to be destroyed.
struct te_widget* text_edit_widget_get_widget(te_text_edit_widget* text_edit_widget);

// Sets height of the text in range [0.0; 1.0] relative to window height.
void text_edit_widget_set_text_height(te_text_edit_widget* text_edit_widget, float height);
float text_edit_widget_get_text_height(te_text_edit_widget* text_edit_widget);

// Sets RGBA color of the text.
void text_edit_widget_set_color(te_text_edit_widget* text_edit_widget, vec4 color);
void text_edit_widget_get_color(te_text_edit_widget* text_edit_widget, vec4 out);
