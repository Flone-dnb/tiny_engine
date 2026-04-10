varying vec3 frag_pos;
varying vec3 frag_normal;
varying vec2 frag_uv;

uniform vec4 color;
uniform vec2 tiling; // stores -1 if texture not set
uniform vec2 uv_offset;
uniform sampler2D model_texture;

void main(void) {
    // Normals may be unnormalized after the rasterization (when they are interpolated).
    vec3 frag_normal_unit = normalize(frag_normal);

    vec4 out_color = color;
    if (tiling.x > 0.0) {
        out_color *= texture2D(model_texture, (frag_uv + uv_offset) * tiling);
    }
    if (out_color.a < 0.1) {
        discard;
    }

    // TODO: dummy normal usage (before lighting is implemented)
    if (frag_normal.x < -50.0){
        out_color.r = 0.0;
    }

    gl_FragColor = out_color;
} 
