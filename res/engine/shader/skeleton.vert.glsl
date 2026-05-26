// same as in the C code
#define TE_MAX_BONE_COUNT 80

attribute vec3 pos;
attribute vec3 normal;
attribute vec2 uv;
attribute vec4 bone_indices;
attribute vec4 bone_weights;

varying vec3 frag_pos;
varying vec3 frag_normal;
varying vec2 frag_uv;
varying vec3 view_space_pos;

uniform mat4 view_mat;
uniform mat4 view_proj_mat;
uniform mat4 world_mat;
uniform mat3 normal_mat;
uniform mat4 skinning_mats[TE_MAX_BONE_COUNT];

void main(void) {
    // up to 4 bones might affect a vertex
    vec4 skinned_pos = vec4(0.0);
    vec4 skinned_normal = vec4(0.0);
    for (int i = 0; i < 4; i++) {
        float bone_weight = bone_weights[i];
        mat4 bone_mat = skinning_mats[int(bone_indices)];

        // passing 0 as 4th component for position to avoid applying translation twice
        skinned_pos += (bone_mat * vec4(pos, 0.0)) * bone_weight;
        skinned_normal += (bone_mat * vec4(normal, 0.0)) * bone_weight;
    }
    vec3 posModelSpace = skinned_pos.xyz;
    vec3 normalModelSpace = skinned_normal.xyz;

    vec4 world_pos = world_mat * vec4(posModelSpace, 1.0);
    gl_Position = view_proj_mat * world_pos;

    view_space_pos = (view_mat * world_pos).xyz;

    frag_pos = world_pos.xyz;
    frag_normal = normal_mat * normalModelSpace;
    frag_uv = uv;
}
