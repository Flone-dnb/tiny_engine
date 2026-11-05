#pragma once

#include <stdbool.h>
#include <math/vec2.h>
#include <math/vec4.h>

typedef struct te_progress_widget te_progress_widget;
struct te_widget;

te_progress_widget* progress_widget_create(void);
void progress_widget_destroy(te_progress_widget* progress_widget);

/* returns component used to adjust common widget properties (pos, size)
 * and attach widget to other widgets
 * you can destroy returned object and it will cause this widget to be destroyed */
struct te_widget* progress_widget_get_widget(te_progress_widget* progress_widget);

/* sets progress state in range [0.0; 1.0] */
void progress_widget_set_value(te_progress_widget* progress_widget, float value);
float progress_widget_get_value(te_progress_widget* progress_widget);

/* sets RGBA color of the elements */
void progress_widget_set_background_color(te_progress_widget* progress_widget, te_vec4 color);
void progress_widget_set_foreground_color(te_progress_widget* progress_widget, te_vec4 color);
void progress_widget_get_background_color(te_progress_widget* progress_widget, te_vec4 out);
void progress_widget_get_foreground_color(te_progress_widget* progress_widget, te_vec4 out);

/* sets path (relative to the `res` directory) to the texture of the widget elements
 * specify NULL to remove texture. The string is copied to the widget's internal data */
void progress_widget_set_background_texture(
    te_progress_widget* progress_widget, const char* relative_path);
void progress_widget_set_foreground_texture(
    te_progress_widget* progress_widget, const char* relative_path);

/* return NULL if texture is not set
 * do not free returned string, valid while the slider exists and the texture is not changed */
char* progress_widget_get_background_texture(te_progress_widget* progress_widget);
char* progress_widget_get_foreground_texture(te_progress_widget* progress_widget);

/* sets linear or nearest (default) texture filtering */
void progress_widget_set_texture_linear_filtering(
    te_progress_widget* progress_widget, bool linear_filtering);
bool progress_widget_get_texture_linear_filtering(te_progress_widget* progress_widget);

/* if texture is used specifies offset and size in the referenced texture to display
 * value in range [0.0; 1.0] relative to the texture size
 * this is useful when a texture contains multiple elements and 1 widgets
 * needs to reference only 1 element from the texture */
void progress_widget_set_uv_offset(te_progress_widget* progress_widget, te_vec2 uv_offset);
void progress_widget_get_uv_offset(te_progress_widget* progress_widget, te_vec2 out);
void progress_widget_set_uv_size(te_progress_widget* progress_widget, te_vec2 uv_size);
void progress_widget_get_uv_size(te_progress_widget* progress_widget, te_vec2 out);

void progress_widget_set_uv_offset_foreground(
    te_progress_widget* progress_widget, te_vec2 uv_offset);
void
progress_widget_get_uv_offset_foreground(te_progress_widget* progress_widget, te_vec2 out);
void
progress_widget_set_uv_size_foreground(te_progress_widget* progress_widget, te_vec2 uv_size);
void progress_widget_get_uv_size_foreground(te_progress_widget* progress_widget, te_vec2 out);

/* returns unique ID of this type in the type database */
const char* progress_widget_get_type_id(void);
/* registers the type in the type database */
void progress_widget_register_type(void);
