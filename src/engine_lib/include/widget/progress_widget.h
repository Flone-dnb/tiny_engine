#pragma once

#include <cglm/vec4.h>

typedef struct te_progress_widget te_progress_widget;
struct te_widget;

te_progress_widget* progress_widget_create(void);
void progress_widget_destroy(te_progress_widget* progress_widget);

// Returns component used to adjust common widget properties (pos, size)
// and attach widget to other widgets.
// You can destroy returned object and it will cause this widget to be destroyed.
struct te_widget* progress_widget_get_widget(te_progress_widget* progress_widget);

// Sets progress state in range [0.0; 1.0].
void progress_widget_set_value(te_progress_widget* progress_widget, float value);
float progress_widget_get_value(te_progress_widget* progress_widget);

// Sets RGBA color of the elements.
void progress_widget_set_background_color(te_progress_widget* progress_widget, vec4 color);
void progress_widget_set_foreground_color(te_progress_widget* progress_widget, vec4 color);
void progress_widget_get_background_color(te_progress_widget* progress_widget, vec4 out);
void progress_widget_get_foreground_color(te_progress_widget* progress_widget, vec4 out);

// Sets path (relative to the `res` directory) to the texture of the widget elements.
// Specify NULL to remove texture. The string is copied to the widget's internal data.
void progress_widget_set_background_texture(
    te_progress_widget* progress_widget, const char* relative_path);
void progress_widget_set_foreground_texture(
    te_progress_widget* progress_widget, const char* relative_path);

// Return NULL if texture is not set.
// Do not free returned string, valid while the slider exists and the texture is not changed.
char* progress_widget_get_background_texture(te_progress_widget* progress_widget);
char* progress_widget_get_foreground_texture(te_progress_widget* progress_widget);

// Returns unique ID of this type in the type database.
const char* progress_widget_get_type_id(void);
// Registers the type in the type database.
void progress_widget_register_type(void);
