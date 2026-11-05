#pragma once

#include <stdbool.h>
#include <math/vec2.h>
#include <math/vec4.h>

typedef struct te_button_widget te_button_widget;
struct te_widget;

te_button_widget* button_widget_create(void);
void button_widget_destroy(te_button_widget* button_widget);

/* returns component used to adjust common widget properties (pos, size)
 * and attach widget to other widgets
 * you can destroy returned object and it will cause this widget to be destroyed */
struct te_widget* button_widget_get_widget(te_button_widget* button_widget);

void button_widget_set_on_clicked(
    te_button_widget* button_widget, void (*on_clicked)(te_button_widget*));
void button_widget_set_on_right_clicked(
    te_button_widget* button_widget, void (*on_right_clicked)(te_button_widget*));

/* sets RGBA colors for 3 states: normal, hovered and pressed */
void button_widget_set_color(te_button_widget* button_widget, te_vec4 color);
void button_widget_set_color_hovered(te_button_widget* button_widget, te_vec4 color);
void button_widget_set_color_pressed(te_button_widget* button_widget, te_vec4 color);

void button_widget_get_color(te_button_widget* button_widget, te_vec4 out);
void button_widget_get_color_hovered(te_button_widget* button_widget, te_vec4 out);
void button_widget_get_color_pressed(te_button_widget* button_widget, te_vec4 out);

/* sets textures (path relative to the "res" directory) for 3 states: normal, hovered and pressed
 * specify NULL to remove texture. The string will be copied to the button */
void button_widget_set_texture(te_button_widget* button_widget, const char* relative_path);
void
button_widget_set_texture_hovered(te_button_widget* button_widget, const char* relative_path);
void
button_widget_set_texture_pressed(te_button_widget* button_widget, const char* relative_path);

/* NULL if not set, do not free returned string, valid while the button exists and the texture is not changed */
char* button_widget_get_texture(te_button_widget* button_widget);
char* button_widget_get_texture_hovered(te_button_widget* button_widget);
char* button_widget_get_texture_pressed(te_button_widget* button_widget);

/* sets linear or nearest (default) texture filtering */
void button_widget_set_texture_linear_filtering(
    te_button_widget* button_widget, bool linear_filtering);
bool button_widget_get_texture_linear_filtering(te_button_widget* button_widget);

/* if texture is used specifies offset and size in the referenced texture to display
 * value in range [0.0; 1.0] relative to the texture size
 * this is useful when a texture contains multiple elements and 1 widgets
 * needs to reference only 1 element from the texture */
void button_widget_set_uv_offset(te_button_widget* button_widget, te_vec2 uv_offset);
void button_widget_get_uv_offset(te_button_widget* button_widget, te_vec2 out);
void button_widget_set_uv_size(te_button_widget* button_widget, te_vec2 uv_size);
void button_widget_get_uv_size(te_button_widget* button_widget, te_vec2 out);

void button_widget_set_uv_offset_hovered(te_button_widget* button_widget, te_vec2 uv_offset);
void button_widget_get_uv_offset_hovered(te_button_widget* button_widget, te_vec2 out);
void button_widget_set_uv_size_hovered(te_button_widget* button_widget, te_vec2 uv_size);
void button_widget_get_uv_size_hovered(te_button_widget* button_widget, te_vec2 out);

void button_widget_set_uv_offset_pressed(te_button_widget* button_widget, te_vec2 uv_offset);
void button_widget_get_uv_offset_pressed(te_button_widget* button_widget, te_vec2 out);
void button_widget_set_uv_size_pressed(te_button_widget* button_widget, te_vec2 uv_size);
void button_widget_get_uv_size_pressed(te_button_widget* button_widget, te_vec2 out);

/* used internally but can be used externally to create visual feedback when creating UI navigation using a gamepad */
void button_widget_enter_normal_state(te_button_widget* button_widget);
void button_widget_enter_hovered_state(te_button_widget* button_widget);
void button_widget_enter_pressed_state(te_button_widget* button_widget);

/* returns unique ID of this type in the type database */
const char* button_widget_get_type_id(void);
/* registers the type in the type database */
void button_widget_register_type(void);
