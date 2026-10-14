ATTRIBUTE_IN vec3 local_pos;

uniform mat4 view_proj_mat;
uniform vec3 v0;
uniform vec3 v1;
uniform vec3 v2;

void main() {
    vec3 vert = local_pos.x < 1.0 ? v0 : local_pos.x < 2.0 ? v1 : v2;
    gl_Position = view_proj_mat * vec4(vert, 1.0);
}
