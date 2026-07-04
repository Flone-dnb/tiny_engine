in vec3 frag_pos;
in vec3 frag_normal;
in vec2 frag_uv;
in vec3 view_space_pos;

uniform vec4 model_color;
uniform vec2 tiling; // stores -1 if texture not set
uniform vec2 uv_offset;
uniform sampler2D model_texture;

// lighting
uniform vec4 directional_light_color; // color in RGB and intensity in A
uniform vec4 point_light_color; // color in RGB and intensity in A
uniform vec4 point_light_pos_and_dist; // position in XYZ and light radius in W
uniform vec3 directional_light_direction;
uniform vec3 ambient_light_color;
uniform vec3 distance_fog_color;
uniform vec2 distance_fog_range; // start/end distance from camera

out vec4 out_color;

vec3 calculate_light_color(vec3 frag_pos, vec3 frag_normal_unit, vec3 model_color) {
    vec3 light = ambient_light_color * model_color;

    // Directional light.
    float cos_frag_to_light = max(dot(frag_normal_unit, -directional_light_direction), 0.0);
    light += model_color * ((directional_light_color.rgb * directional_light_color.a) * cos_frag_to_light);

    // Point light.
    cos_frag_to_light = max(dot(frag_normal_unit, normalize(point_light_pos_and_dist.xyz - frag_pos)), 0.0);
    float attenuation = clamp(
        (point_light_pos_and_dist.w - length(point_light_pos_and_dist.xyz - frag_pos)) / point_light_pos_and_dist.w, 0.0, 1.0)
        * point_light_color.a;
    light += model_color * ((point_light_color.rgb * attenuation) * cos_frag_to_light);

    return light;
}

void main(void) {
    // Normals may be unnormalized after the rasterization (when they are interpolated).
    vec3 frag_normal_unit = normalize(frag_normal);

    // Get color.
    vec4 color = model_color;
    if (tiling.x > 0.0) {
        color *= texture2D(model_texture, (frag_uv + uv_offset) * tiling);
    }
    if (color.a < 0.1) {
        discard;
    }

    vec3 light_color = calculate_light_color(frag_pos, frag_normal_unit, color.rgb);

    // Distance fog.
    if (distance_fog_range.x >= 0.0) {
        float fog_portion = smoothstep(distance_fog_range.x, distance_fog_range.y, length(view_space_pos));
        light_color = mix(light_color, distance_fog_color, fog_portion);
    }

    out_color = vec4(light_color.rgb, color.a);
} 
