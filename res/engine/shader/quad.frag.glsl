varying vec2 fragment_uv;

uniform vec4 quad_color;
uniform bool is_using_tex;
uniform sampler2D quad_tex;

void main() {
    gl_FragColor = quad_color;
    if (is_using_tex) {
        gl_FragColor *= texture2D(quad_tex, fragment_uv);
    }
}
