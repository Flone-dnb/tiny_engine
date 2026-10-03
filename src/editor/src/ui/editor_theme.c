#include <ui/editor_theme.h>

float
prv_theme_vertical_to_horizontal_ratio(void) {
    return 0.6f;
}

float
theme_get_left_panel_width(void) {
    return 0.11f;
}

float
theme_get_right_panel_width(void) {
    return 0.11f;
}

void
theme_get_background_panel_color(vec4 rgba) {
    glm_vec4_copy((vec4){0.15f, 0.15f, 0.15f, 1.0f}, rgba);
}

float
theme_get_world_inspector_height(void) {
    return 0.7f;
}

float
theme_get_text_height(void) {
    return 0.01875f;
}

float
theme_get_horizontal_padding(void) {
    return 0.003f;
}

float
theme_get_vertical_padding(void) {
    return theme_get_horizontal_padding() * prv_theme_vertical_to_horizontal_ratio() * 3.0f;
}

float
theme_get_horizontal_spacing(void) {
    return 0.0025f;
}

float
theme_get_vertical_spacing(void) {
    return theme_get_horizontal_spacing() * prv_theme_vertical_to_horizontal_ratio() * 3.0f;
}

float
theme_get_button_height(void) {
    return 0.027f;
}

void
theme_get_accent_color(vec4 rgba) {
    glm_vec4_copy((vec4){0.85f, 0.35f, 0.2f, 1.0f}, rgba);
}

void
theme_get_button_color(vec4 rgba) {
    glm_vec4_copy((vec4){0.225f, 0.225f, 0.225f, 1.0f}, rgba);
}

void
theme_get_button_color_hovered(vec4 rgba) {
    theme_get_button_color(rgba);
    glm_vec4_adds(rgba, 0.2f, rgba);
}

void
theme_get_button_color_pressed(vec4 rgba) {
    theme_get_button_color(rgba);
    glm_vec4_adds(rgba, 0.1f, rgba);
}

void
theme_get_text_edit_background_color(vec4 rgba) {
    theme_get_button_color(rgba);
}
