ATTRIBUTE_IN vec3 pos;
ATTRIBUTE_IN vec3 normal;
ATTRIBUTE_IN vec2 uv;

ATTRIBUTE_OUT vec3 frag_pos;
ATTRIBUTE_OUT vec3 frag_normal;
ATTRIBUTE_OUT vec2 frag_uv;
ATTRIBUTE_OUT vec3 view_space_pos;

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
} 
