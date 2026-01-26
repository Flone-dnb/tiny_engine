varying vec3 frag_pos;
varying vec3 frag_normal;
varying vec2 frag_uv;

uniform vec4 color;

void main(void) {
    // Normals may be unnormalized after the rasterization (when they are interpolated).
    vec3 frag_normal_unit = normalize(frag_normal);
 
    gl_FragColor = color;
} 
