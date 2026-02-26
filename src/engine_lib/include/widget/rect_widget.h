#pragma once

#include "cglm/vec4.h"

// Rectangular widget, displays a color or a texture.
typedef struct te_rect_widget te_rect_widget;
struct te_widget;

te_rect_widget* rect_widget_create(void);
void rect_widget_destroy(te_rect_widget* rect_widget);

// Returns component used to adjust common widget properties (pos, size)
// and attach widget to other widgets.
// You can destroy returned object and it will cause this widget to be destroyed.
struct te_widget* rect_widget_get_widget(te_rect_widget* rect_widget);

// Sets RGBA color of the rectangle.
void rect_widget_set_color(te_rect_widget* rect_widget, vec4 color);
void rect_widget_get_color(te_rect_widget* rect_widget, vec4 out);

// Sets path (relative to the `res` directory) to texture to use.
// The path string will be copied and stored in the model. Specify NULL to remove texture.
void rect_widget_set_texture(te_rect_widget* rect_widget, const char* relative_path);

// Returns NULL if texture is not set, otherwise path (relative to the `res` directory) to the used texture.
// Do not free returned string, valid while the model exists.
const char* rect_widget_get_texture(te_rect_widget* rect_widget);

// Allows "cutting" part of the rectangle during the rendering.
// XY stores clip start in range [0.0; 1.0] and ZW stores clip size in the same range.
void rect_widget_set_clip_rect(te_rect_widget* rect_widget, vec4 clip_rect);
void rect_widget_get_clip_rect(te_rect_widget* rect_widget, vec4 out);

// Returns unique ID of this type in the type database.
const char* rect_widget_get_type_id(void);
// Registers the type in the type database.
void rect_widget_register_type(void);