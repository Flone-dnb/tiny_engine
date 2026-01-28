varying vec3 frag_pos;
varying vec3 frag_normal;
varying vec2 frag_uv;

uniform vec4 color;

void main(void) {
    // Normals may be unnormalized after the rasterization (when they are interpolated).
    vec3 frag_normal_unit = normalize(frag_normal);

	vec4 out_color = color;

    // TODO: dummy usage
    if (frag_normal.x < -5.0 || frag_uv.x < 0.5) {
		out_color.r += 0.5;
    }
 
    gl_FragColor = out_color;
} 
