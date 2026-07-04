in vec2 fragment_uv;

uniform vec4 quad_color;
uniform bool is_using_tex;
uniform sampler2D quad_tex;

out vec4 out_color;

void main() {
    out_color = quad_color;
    if (is_using_tex) {
        out_color *= texture2D(quad_tex, fragment_uv);
    }
}
