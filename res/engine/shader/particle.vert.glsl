ATTRIBUTE_IN vec2 in_quad_uv;

ATTRIBUTE_OUT vec4 particle_color;
ATTRIBUTE_OUT vec2 particle_uv;

uniform mat4 view_mat;
uniform mat4 proj_mat;
uniform vec4 in_color;
uniform vec4 in_world_pos_size; // size in W

void main() {
    uv = vec2(in_quad_uv.x, 1.0 - in_quad_uv.y);

    vec4 particle_view_pos = view_mat * vec4(in_world_pos_size.xyz, 1.0);
    vec3 up = vec3(view_mat * vec4(0.0, 1.0, 0.0, 0.0));
    vec3 right = normalize(cross(up, -particle_view_pos.xyz));

    vec2 vert_offset = in_quad_uv - vec2(0.5, 0.5);
    vert_offset *= in_world_pos_size.w / 2.0;

    vec3 vert_view_pos = particle_view_pos.xyz;
    vert_view_pos += vert_offset.x * right;
    vert_view_pos += vert_offset.y * up;

    gl_Position = proj_mat * vec4(vert_view_pos, particle_view_pos.w);
}
