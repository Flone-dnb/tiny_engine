#pragma once

#include <stdbool.h>
#include <math/vec2.h>
#include <math/vec4.h>

typedef struct te_checkbox_widget te_checkbox_widget;
struct te_widget;

te_checkbox_widget* checkbox_widget_create(void);
void checkbox_widget_destroy(te_checkbox_widget* checkbox_widget);

/* returns component used to adjust common widget properties (pos, size)
 * and attach widget to other widgets.
 * you can destroy returned object and it will cause this widget to be destroyed */
struct te_widget* checkbox_widget_get_widget(te_checkbox_widget* checkbox_widget);

void checkbox_widget_set_on_changed(
    te_checkbox_widget* checkbox_widget,
    void (*on_changed)(te_checkbox_widget* checkbox_widget, bool is_checked));

void checkbox_widget_set_is_checked(te_checkbox_widget* checkbox_widget, bool is_checked);
bool checkbox_widget_is_checked(te_checkbox_widget* checkbox_widget);

/* sets RGBA color to checkbox background and foreground (checked state) */
void checkbox_widget_set_background_color(te_checkbox_widget* checkbox_widget, te_vec4 color);
void checkbox_widget_set_checked_color(te_checkbox_widget* checkbox_widget, te_vec4 color);

void checkbox_widget_get_background_color(te_checkbox_widget* checkbox_widget, te_vec4 out);
void checkbox_widget_get_checked_color(te_checkbox_widget* checkbox_widget, te_vec4 out);

/* sets path (relataive to the `res` directory) to background and foreground (checked) textures */
void checkbox_widget_set_background_texture(
    te_checkbox_widget* checkbox_widget, const char* relative_path);
void checkbox_widget_set_checked_texture(
    te_checkbox_widget* checkbox_widget, const char* relative_path);

/* returns NULL if the texture was not set
 * do not free returned string, valid while the widget exists and the texture is not changed */
char* checkbox_widget_get_background_texture(te_checkbox_widget* checkbox_widget);
char* checkbox_widget_get_checked_texture(te_checkbox_widget* checkbox_widget);

/* if texture is used specifies offset and size in the referenced texture to display
 * value in range [0.0; 1.0] relative to the texture size
 * this is useful when a texture contains multiple elements and 1 widgets
 * needs to reference only 1 element from the texture */
void checkbox_widget_set_uv_offset(te_checkbox_widget* checkbox_widget, te_vec2 uv_offset);
void checkbox_widget_get_uv_offset(te_checkbox_widget* checkbox_widget, te_vec2 out);
void checkbox_widget_set_uv_size(te_checkbox_widget* checkbox_widget, te_vec2 uv_size);
void checkbox_widget_get_uv_size(te_checkbox_widget* checkbox_widget, te_vec2 out);

void
checkbox_widget_set_uv_offset_checked(te_checkbox_widget* checkbox_widget, te_vec2 uv_offset);
void checkbox_widget_get_uv_offset_checked(te_checkbox_widget* checkbox_widget, te_vec2 out);
void checkbox_widget_set_uv_size_checked(te_checkbox_widget* checkbox_widget, te_vec2 uv_size);
void checkbox_widget_get_uv_size_checked(te_checkbox_widget* checkbox_widget, te_vec2 out);

/* returns unique ID of this type in the type database */
const char* checkbox_widget_get_type_id(void);
/* registers the type in the type database */
void checkbox_widget_register_type(void);
