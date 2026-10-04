#include <ui/editor_theme.h>

float
prv_editor_theme_vertical_to_horizontal_ratio(void) {
    return 0.6f;
}

float
editor_theme_get_left_panel_width(void) {
    return 0.11f;
}

float
editor_theme_get_right_panel_width(void) {
    return 0.11f;
}

void
editor_theme_get_background_panel_color(te_vec4 rgba) {
    vec4_set(0.15f, 0.15f, 0.15f, 1.0f, rgba);
}

float
editor_theme_get_world_inspector_height(void) {
    return 0.7f;
}

float
editor_theme_get_text_height(void) {
    return 0.01875f;
}

float
editor_theme_get_horizontal_padding(void) {
    return 0.003f;
}

float
editor_theme_get_vertical_padding(void) {
    return editor_theme_get_horizontal_padding() * prv_editor_theme_vertical_to_horizontal_ratio() * 3.0f;
}

float
editor_theme_get_horizontal_spacing(void) {
    return 0.0025f;
}

float
editor_theme_get_vertical_spacing(void) {
    return editor_theme_get_horizontal_spacing() * prv_editor_theme_vertical_to_horizontal_ratio() * 3.0f;
}

float
editor_theme_get_button_height(void) {
    return 0.027f;
}

void
editor_theme_get_accent_color(te_vec4 rgba) {
    vec4_set(0.85f, 0.35f, 0.2f, 1.0f, rgba);
}

void
editor_theme_get_button_color(te_vec4 rgba) {
    vec4_set(0.225f, 0.225f, 0.225f, 1.0f, rgba);
}

void
editor_theme_get_button_color_hovered(te_vec4 rgba) {
    editor_theme_get_button_color(rgba);
    vec4_adds(rgba, 0.2f, rgba);
}

void
editor_theme_get_button_color_pressed(te_vec4 rgba) {
    editor_theme_get_button_color(rgba);
    vec4_adds(rgba, 0.1f, rgba);
}

void
editor_theme_get_text_edit_background_color(te_vec4 rgba) {
    editor_theme_get_button_color(rgba);
}
