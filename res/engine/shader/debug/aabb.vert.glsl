attribute vec3 local_pos;

uniform vec3 pos_offset;
uniform vec3 extents;
uniform mat4 view_proj_mat;

void main() {
    gl_Position = view_proj_mat * vec4((local_pos * extents) + pos_offset, 1.0);
}
