layout (location = 0) in vec3 local_pos;

uniform vec3 from;
uniform vec3 to;
uniform mat4 view_proj_mat;

void main() {
    gl_Position = view_proj_mat * vec4(float(local_pos.x < 0.5) * from + float(local_pos.x > 0.5) * to, 1.0);
}
