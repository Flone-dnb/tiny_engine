#pragma once

#include <stdbool.h>
#include <math/vec2.h>
#include <math/vec4.h>

/* rectangular widget, displays a color or a texture */
typedef struct te_rect_widget te_rect_widget;
struct te_widget;

te_rect_widget* rect_widget_create(void);
void rect_widget_destroy(te_rect_widget* rect_widget);

/* returns component used to adjust common widget properties (pos, size)
 * and attach widget to other widgets
 * you can destroy returned object and it will cause this widget to be destroyed */
struct te_widget* rect_widget_get_widget(te_rect_widget* rect_widget);

/* sets RGBA color of the rectangle */
void rect_widget_set_color(te_rect_widget* rect_widget, te_vec4 color);
void rect_widget_get_color(te_rect_widget* rect_widget, te_vec4 out);

/* sets path (relative to the `res` directory) to texture to use
 * the path string will be copied and stored in the model, specify NULL to remove texture */
void rect_widget_set_texture(te_rect_widget* rect_widget, const char* relative_path);

/* sets linear or nearest (default) texture filtering */
void
rect_widget_set_texture_linear_filtering(te_rect_widget* rect_widget, bool linear_filtering);
bool rect_widget_get_texture_linear_filtering(te_rect_widget* rect_widget);

/* if texture is used specifies offset and size in the referenced texture to display
 * value in range [0.0; 1.0] relative to the texture size
 * this is useful when a texture contains multiple elements and 1 widgets
 * needs to reference only 1 element from the texture */
void rect_widget_set_uv_offset(te_rect_widget* rect_widget, te_vec2 uv_offset);
void rect_widget_get_uv_offset(te_rect_widget* rect_widget, te_vec2 out);
void rect_widget_set_uv_size(te_rect_widget* rect_widget, te_vec2 uv_size);
void rect_widget_get_uv_size(te_rect_widget* rect_widget, te_vec2 out);

/* returns NULL if texture is not set, otherwise path (relative to the `res` directory) to the used texture
 * do not free returned string, valid while the model exists */
const char* rect_widget_get_texture(te_rect_widget* rect_widget);

/* allows "cutting" part of the rectangle during the rendering
 * XY stores clip start in range [0.0; 1.0] and ZW stores clip size in the same range */
void rect_widget_set_clip_rect(te_rect_widget* rect_widget, te_vec4 clip_rect);
void rect_widget_get_clip_rect(te_rect_widget* rect_widget, te_vec4 out);

/* returns unique ID of this type in the type database */
const char* rect_widget_get_type_id(void);
/* registers the type in the type database */
void rect_widget_register_type(void);
