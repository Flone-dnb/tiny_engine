#pragma once

#include <cglm/vec4.h>

static inline float
prv_theme_vertical_to_horizontal_ratio(void) {
    return 0.6f;
}

// Returns width in range [0.0; 1.0] of the left panel (displays world inspector and filesystem panel).
static inline float
theme_get_left_panel_width(void) {
    return 0.125f;
}

// Returns width in range [0.0; 1.0] of the right panel (displays object inspector).
static inline float
theme_get_right_panel_width(void) {
    return 0.125f;
}

static inline void
theme_get_background_panel_color(vec4 rgba) {
    glm_vec4_copy((vec4){0.15f, 0.15f, 0.15f, 1.0f}, rgba);
}

// Returns height in range [0.0; 1.0] (relative to the left panel height) of the world inspector.
static inline float
theme_get_world_inspector_height(void) {
    return 0.7f;
}

static inline float
theme_get_text_height(void) {
    return 0.019f;
}

static inline float
theme_get_horizontal_padding(void) {
    return 0.003f;
}

static inline float
theme_get_vertical_padding(void) {
    return 0.001f;
}

static inline float
theme_get_horizontal_padding_in_button(void) {
    return 0.05f * prv_theme_vertical_to_horizontal_ratio();
}

static inline float
theme_get_vertical_padding_in_button(void) {
    return theme_get_horizontal_padding_in_button() * prv_theme_vertical_to_horizontal_ratio();
}

static inline float
theme_get_horizontal_spacing(void) {
    return 0.002f;
}

static inline float
theme_get_vertical_spacing(void) {
    return theme_get_horizontal_spacing() * prv_theme_vertical_to_horizontal_ratio();
}

static inline float
theme_get_button_height(void) {
    return 0.027f;
}

static inline void
theme_get_accent_color(vec4 rgba) {
    glm_vec4_copy((vec4){0.85f, 0.35f, 0.2f, 1.0f}, rgba);
}

static inline void
theme_get_button_color(vec4 rgba) {
    glm_vec4_copy((vec4){0.225f, 0.225f, 0.225f, 1.0f}, rgba);
}

static inline void
theme_get_button_color_hovered(vec4 rgba) {
    theme_get_button_color(rgba);
    glm_vec4_adds(rgba, 0.2f, rgba);
}

static inline void
theme_get_button_color_pressed(vec4 rgba) {
    theme_get_button_color(rgba);
    glm_vec4_adds(rgba, 0.1f, rgba);
}
