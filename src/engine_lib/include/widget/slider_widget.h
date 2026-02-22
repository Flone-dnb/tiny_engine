#pragma once

#include "cglm/vec4.h"

typedef struct te_slider_widget te_slider_widget;
struct te_widget;

te_slider_widget* slider_widget_create(void);
void slider_widget_destroy(te_slider_widget* slider_widget);

// Returns component used to adjust common widget properties (pos, size)
// and attach widget to other widgets.
// You can destroy returned object and it will cause this widget to be destroyed.
struct te_widget* slider_widget_get_widget(te_slider_widget* slider_widget);

// Sets a callback that will be triggered after the slider's handle is moved by the user input.
void slider_widget_set_on_value_changed(
    te_slider_widget* slider_widget, void (*on_value_changed)(te_slider_widget* slider_widget, float new_value));

// Sets slider's handle position in range [0.0; 1.0].
void slider_widget_set_value(te_slider_widget* slider_widget, float value);
float slider_widget_get_value(te_slider_widget* slider_widget);

// Sets the size of a single movement in slider's handle position in range [0.0; 1.0]. 0 if can move freely.
void slider_widget_set_step_size(te_slider_widget* slider_widget, float step_size);
float slider_widget_get_step_size(te_slider_widget* slider_widget);

// Sets RGBA color of slider elements.
void slider_widget_set_background_color(te_slider_widget* slider_widget, vec4 color);
void slider_widget_set_handle_color(te_slider_widget* slider_widget, vec4 color);

void slider_widget_get_background_color(te_slider_widget* slider_widget, vec4 out);
void slider_widget_get_handle_color(te_slider_widget* slider_widget, vec4 out);

// Sets path (relative to the `res` directory) to the texture of slider elements.
// Specify NULL to remove texture. The string is copied to the slider's internal data.
void slider_widget_set_background_texture(te_slider_widget* slider_widget, const char* relative_path);
void slider_widget_set_handle_texture(te_slider_widget* slider_widget, const char* relative_path);

// Return NULL if texture is not set.
// Do not free returned string, valid while the slider exists and the texture is not changed.
char* slider_widget_get_background_texture(te_slider_widget* slider_widget);
char* slider_widget_get_handle_texture(te_slider_widget* slider_widget);
