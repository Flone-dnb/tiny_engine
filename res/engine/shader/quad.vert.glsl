// Stores position in XY in [0.0; 1.0] and UV in ZW.
attribute vec4 vertex;

uniform vec2 in_pos;      // in pixels
uniform vec2 in_size;     // in pixels
uniform vec4 clip_rect;   // [0.0; 1.0] where XY mark clip start and ZW mark clip size.
uniform vec2 window_size; // in pixels

varying vec2 fragment_uv;

void main() {
    vec2 pos = vec2(in_pos + in_size * clip_rect.xy);
    vec2 size = vec2(in_size * clip_rect.zw);

    pos.y = window_size.y - pos.y; // flip Y origin from our UI to OpenGL

    vec2 relative_pos = vec2(
        (vertex.x *  size.x + pos.x) / window_size.x,
        (vertex.y * -size.y + pos.y) / window_size.y); // flip Y direction from our UI to OpenGL
    vec2 ndc_pos = relative_pos * 2.0 - 1.0;

    gl_Position = vec4(ndc_pos, 0.0, 1.0);
    fragment_uv = vertex.zw * clip_rect.zw + clip_rect.xy;
}
