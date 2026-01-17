#include "ecs/transform_component.h"

#include "misc/globals.h"

/** Allows entities to have a specific location, rotation and size in the world. */
typedef struct te_transform_component {
    /** Location. */
    vec3 location;

    /** Rotation in radians. */
    vec3 rotation;

    /** Scale. */
    vec3 scale;
} te_transform_component;

void
transform_set_location(struct te_transform_component* comp, vec3 in) {
    glm_vec3_copy(in, comp->location);
}

void
transform_set_rotation(struct te_transform_component* comp, vec3 in) {
    glm_vec3_copy(in, comp->rotation);
}

void
transform_set_scale(struct te_transform_component* comp, vec3 in) {
    glm_vec3_copy(in, comp->scale);
}

void
transform_get_location(te_transform_component* comp, vec3 out) {
    out = comp->location;
}

void
transform_get_rotation(te_transform_component* comp, vec3 out) {
    out = comp->rotation;
}

void
transform_get_scale(te_transform_component* comp, vec3 out) {
    out = comp->scale;
}

void
transform_get_forward(struct te_transform_component* comp, vec4 out) {
    mat4 rotation_mat;
    glm_euler(comp->rotation, rotation_mat);

    vec4 global_forward;
    globals_get_world_forward(global_forward);

    glm_mat4_mulv(rotation_mat, global_forward, out);
}

unsigned int
prv_transform_component_get_sizeof(void) {
    return sizeof(te_transform_component);
}

void
prv_transform_component_init(void* data) {
    te_transform_component* comp = data;
    glm_vec3_zero(comp->location);
    glm_vec3_zero(comp->rotation);
    glm_vec3_one(comp->scale);
}
