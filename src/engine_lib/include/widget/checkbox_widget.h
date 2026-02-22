#pragma once

#include <stdbool.h>
#include "cglm/vec4.h"

typedef struct te_checkbox_widget te_checkbox_widget;
struct te_widget;

te_checkbox_widget* checkbox_widget_create(void);
void checkbox_widget_destroy(te_checkbox_widget* checkbox_widget);

// Returns component used to adjust common widget properties (pos, size)
// and attach widget to other widgets.
// You can destroy returned object and it will cause this widget to be destroyed.
struct te_widget* checkbox_widget_get_widget(te_checkbox_widget* checkbox_widget);

void checkbox_widget_set_on_changed(
    te_checkbox_widget* checkbox_widget, void (*on_changed)(te_checkbox_widget* checkbox_widget, bool is_checked));

void checkbox_widget_set_is_checked(te_checkbox_widget* checkbox_widget, bool is_checked);
bool checkbox_widget_is_checked(te_checkbox_widget* checkbox_widget);

// Sets RGBA color to checkbox background and foreground (checked state).
void checkbox_widget_set_background_color(te_checkbox_widget* checkbox_widget, vec4 color);
void checkbox_widget_set_checked_color(te_checkbox_widget* checkbox_widget, vec4 color);

void checkbox_widget_get_background_color(te_checkbox_widget* checkbox_widget, vec4 out);
void checkbox_widget_get_checked_color(te_checkbox_widget* checkbox_widget, vec4 out);

// Sets path (relataive to the `res` directory) to background and foreground (checked) textures.
void checkbox_widget_set_background_texture(te_checkbox_widget* checkbox_widget, const char* relative_path);
void checkbox_widget_set_checked_texture(te_checkbox_widget* checkbox_widget, const char* relative_path);

// Returns NULL if the texture was not set.
// Do not free returned string, valid while the widget exists and the texture is not changed.
char* checkbox_widget_get_background_texture(te_checkbox_widget* checkbox_widget);
char* checkbox_widget_get_checked_texture(te_checkbox_widget* checkbox_widget);
