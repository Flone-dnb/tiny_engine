#pragma once

#include <cglm/vec4.h>
#include <cglm/vec3.h>

float prv_theme_vertical_to_horizontal_ratio(void);

/* returns width in range [0.0; 1.0] of the left panel (displays world inspector and filesystem panel) */
float theme_get_left_panel_width(void);

/* returns width in range [0.0; 1.0] of the right panel (displays object inspector) */
float theme_get_right_panel_width(void);

void theme_get_background_panel_color(vec4 rgba);

/* returns height in range [0.0; 1.0] (relative to the left panel height) of the world inspector */
float theme_get_world_inspector_height(void);

float theme_get_text_height(void);

float theme_get_horizontal_padding(void);

float theme_get_vertical_padding(void);

float theme_get_horizontal_spacing(void);

float theme_get_vertical_spacing(void);

float theme_get_button_height(void);

void theme_get_accent_color(vec4 rgba);

void theme_get_button_color(vec4 rgba);

void theme_get_button_color_hovered(vec4 rgba);

void theme_get_button_color_pressed(vec4 rgba);

void theme_get_text_edit_background_color(vec4 rgba);
