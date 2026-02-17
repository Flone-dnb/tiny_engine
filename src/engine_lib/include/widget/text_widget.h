#pragma once

#include <stdbool.h>
#include <wchar.h>
#include "cglm/vec4.h"

typedef struct te_text_widget te_text_widget;
struct te_widget;

te_text_widget* text_widget_create(void);
void text_widget_destroy(te_text_widget* text_widget);

// Returns component used to adjust common widget properties (pos, size)
// and attach widget to other widgets.
// You can destroy returned object and it will cause this widget to be destroyed.
struct te_widget* text_widget_get_widget(te_text_widget* text_widget);

// Text will be copied to the widget's data.
void text_widget_set_text(te_text_widget* text_widget, const wchar_t* text);
// Do not free returned string pointer, valid while the text is not changed and the widget is not destroyed.
wchar_t* text_widget_get_text(te_text_widget* text_widget);

// Moves the ownership of the text to the widget.
void text_widget_set_text_own(te_text_widget* text_widget, wchar_t* text, unsigned int strlen);

// Sets RGBA color of the text.
void text_widget_set_color(te_text_widget* text_widget, vec4 color);
void text_widget_get_color(te_text_widget* text_widget, vec4 out);

// Sets height of the text in range [0.0; 1.0] relative to window height.
void text_widget_set_text_height(te_text_widget* text_widget, float height);
float text_widget_get_text_height(te_text_widget* text_widget);

// Sets vertical space between lines of text, in range [0.0f; +inf] relative to the height of the text.
// Only used when @ref text_widget_set_is_multiline is enabled.
void text_widget_set_line_spacing(te_text_widget* text_widget, float spacing);
float text_widget_get_line_spacing(te_text_widget* text_widget);

void text_widget_set_is_multiline(te_text_widget* text_widget, bool is_multiline);
bool text_widget_is_multiline(te_text_widget* text_widget);
