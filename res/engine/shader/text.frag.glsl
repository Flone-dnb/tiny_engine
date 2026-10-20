ATTRIBUTE_IN vec2 fragment_uv;

uniform sampler2D font_bitmap; // single-channel bitmap
uniform vec4 text_color;
uniform vec2 glyph_rel_size;   // size relative to font bitmap
uniform vec2 glyph_rel_offset; // offset in font bitmap

void main() {
    vec2 uv = fragment_uv * glyph_rel_size + glyph_rel_offset;
    out_color = vec4(text_color.r, text_color.g, text_color.b, texture(font_bitmap, uv).r * text_color.a);
}
