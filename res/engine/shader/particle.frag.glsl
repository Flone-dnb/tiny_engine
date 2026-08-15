ATTRIBUTE_IN vec4 particle_color;
ATTRIBUTE_IN vec2 particle_uv;

uniform bool is_using_tex;
uniform sampler2D particle_tex;

void main(void) {
    out_color = particle_color;
    if (is_using_tex) {
        out_color *= texture(particle_tex, particle_uv);
    }
}
