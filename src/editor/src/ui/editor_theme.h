#pragma once

#include <math/vec4.h>
#include <math/vec3.h>

/* returns width in range [0.0; 1.0] of the left panel (displays world inspector and filesystem panel) */
float editor_theme_get_left_panel_width(void);

/* returns width in range [0.0; 1.0] of the right panel (displays object inspector) */
float editor_theme_get_right_panel_width(void);

void editor_theme_get_background_panel_color(te_vec4 rgba);

/* returns height in range [0.0; 1.0] (relative to the left panel height) of the world inspector */
float editor_theme_get_world_inspector_height(void);

float editor_theme_get_text_height(void);

float editor_theme_get_horizontal_padding(void);

float editor_theme_get_vertical_padding(void);

float editor_theme_get_horizontal_spacing(void);

float editor_theme_get_vertical_spacing(void);

float editor_theme_get_button_height(void);

void editor_theme_get_accent_color(te_vec4 rgba);

void editor_theme_get_button_color(te_vec4 rgba);

void editor_theme_get_button_color_hovered(te_vec4 rgba);

void editor_theme_get_button_color_pressed(te_vec4 rgba);

void editor_theme_get_text_edit_background_color(te_vec4 rgba);

float prv_editor_theme_vertical_to_horizontal_ratio(void);
