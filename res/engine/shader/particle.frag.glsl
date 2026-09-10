ATTRIBUTE_IN vec2 particle_uv;
ATTRIBUTE_IN vec3 view_space_pos;

uniform vec4 particle_color;
uniform bool is_using_tex;
uniform sampler2D particle_tex;
uniform vec3 distance_fog_color;
uniform vec2 distance_fog_range; // start/end distance from camera

void main(void) {
    out_color = particle_color;
    if (is_using_tex) {
        out_color *= texture(particle_tex, particle_uv);
    }

    // Distance fog.
    if (distance_fog_range.x >= 0.0) {
        float fog_portion = smoothstep(distance_fog_range.x, distance_fog_range.y, length(view_space_pos));
        out_color.rgb = mix(out_color.rgb, distance_fog_color, fog_portion);
    }
}
