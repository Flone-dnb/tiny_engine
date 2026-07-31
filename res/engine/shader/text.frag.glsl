ATTRIBUTE_IN vec2 fragment_uv;

uniform sampler2D glyph_bitmap; // single-channel bitmap
uniform vec4 text_color;

void main() {
    out_color = vec4(text_color.r, text_color.g, text_color.b, texture2D(glyph_bitmap, fragment_uv).r * text_color.a);
}
