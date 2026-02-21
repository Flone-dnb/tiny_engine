#pragma once

#include "cglm/vec4.h"

typedef struct te_button_widget te_button_widget;
struct te_widget;

te_button_widget* button_widget_create(void);
void button_widget_destroy(te_button_widget* button_widget);

// Returns component used to adjust common widget properties (pos, size)
// and attach widget to other widgets.
// You can destroy returned object and it will cause this widget to be destroyed.
struct te_widget* button_widget_get_widget(te_button_widget* button_widget);

void button_widget_set_on_clicked(te_button_widget* button_widget, void (*on_clicked)(te_button_widget*));

// Sets RGBA colors for 3 states: normal, hovered and pressed.
void button_widget_set_color(te_button_widget* button_widget, vec4 color);
void button_widget_set_color_hovered(te_button_widget* button_widget, vec4 color);
void button_widget_set_color_pressed(te_button_widget* button_widget, vec4 color);

void button_widget_get_color(te_button_widget* button_widget, vec4 out);
void button_widget_get_color_hovered(te_button_widget* button_widget, vec4 out);
void button_widget_get_color_pressed(te_button_widget* button_widget, vec4 out);

// Sets textures (path relative to the "res" directory) for 3 states: normal, hovered and pressed.
// Specify NULL to remove texture. The string will be copied to the button.
void button_widget_set_texture(te_button_widget* button_widget, const char* relative_path);
void button_widget_set_texture_hovered(te_button_widget* button_widget, const char* relative_path);
void button_widget_set_texture_pressed(te_button_widget* button_widget, const char* relative_path);

// NULL if not set. Do not free returned string, valid while the button exists and the texture is not changed.
char* button_widget_get_texture(te_button_widget* button_widget);
char* button_widget_get_texture_hovered(te_button_widget* button_widget);
char* button_widget_get_texture_pressed(te_button_widget* button_widget);

// Used internally but can be used externally to create visual feedback when creating UI navigation using a gamepad.
void button_widget_enter_normal_state(te_button_widget* button_widget);
void button_widget_enter_hovered_state(te_button_widget* button_widget);
void button_widget_enter_pressed_state(te_button_widget* button_widget);
