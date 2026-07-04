layout (location = 0) in vec3 pos;
layout (location = 0) in vec3 normal;
layout (location = 0) in vec2 uv;

out vec3 frag_pos;
out vec3 frag_normal;
out vec2 frag_uv;
out vec3 view_space_pos;

uniform mat4 view_mat;
uniform mat4 view_proj_mat;
uniform mat4 world_mat;
uniform mat3 normal_mat;

void main(void) {
    vec4 world_pos = world_mat * vec4(pos, 1.0);
    gl_Position = view_proj_mat * world_pos;

    view_space_pos = (view_mat * world_pos).xyz;

    frag_pos = world_pos.xyz;
    frag_normal = normal_mat * normal;
    frag_uv = uv;

    gl_Position.z = -1.0;
}
