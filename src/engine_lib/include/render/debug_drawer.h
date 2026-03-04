#pragma once

#if defined(ENGINE_DEBUG_TOOLS)

#include <cglm/vec2.h>
#include <cglm/vec3.h>

struct te_renderer;

// Draws a text for a certain amount of time. Specify 0 as time to draw for just 1 frame.
// The text string is copied.
//
// The text will appear in the top-left corner of the screen automatically
// adjusted if there is already some text being displayed this way
// (so multiple text object won't be drawn on top of each other),
// otherwise specify a position (relative to the window's top-left corner) in range [0.0; 1.0].
void debug_drawer_draw_text(const char* text, float time_sec);
void debug_drawer_draw_text_color(const char* text, float time_sec, vec3 color);
void debug_drawer_draw_text_color_pos(const char* text, float time_sec, vec3 color, vec2 pos);

float debug_drawer_get_default_text_height();

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Must be called before game is started and after game is finished.
// Must be destroyed before renderer.
void prv_debug_drawer_init(struct te_renderer* renderer);
void prv_debug_drawer_deinit(struct te_renderer* renderer);

// Must be called every frame to draw debug objects.
void prv_debug_drawer_draw(struct te_renderer* renderer, float delta_time_sec);

#endif
