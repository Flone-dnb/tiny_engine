#include "ecs/camera_component.h"
#include "cglm/util.h"

/** Manages camera properties. */
typedef struct te_camera_component {
    /** Distance from camera (view) space origin to camera's near clip plane. */
    float near_clip;

    /** Distance to camera's far clip plane. */
    float far_clip;

    /** Vertical field of view. */
    unsigned char vertical_fov;

    /**
     * `true` if some of the camera properties were changed
     * (so the system will recalculate camera matrices).
     */
    bool is_properties_changed;
} te_camera_component;

void
camera_set_near_clip(te_camera_component* comp, float z) {
    comp->near_clip = glm_max(z, 0.01f);
    comp->is_properties_changed = true;
}

void
camera_set_far_clip(te_camera_component* comp, float z) {
    comp->far_clip = glm_max(z, comp->near_clip + 1.0f);
    comp->is_properties_changed = true;
}

void
camera_set_vertical_fov(struct te_camera_component* comp, unsigned char fov) {
    comp->vertical_fov = fov;
    comp->is_properties_changed = true;
}

float
camera_get_near_clip(te_camera_component* comp) {
    return comp->near_clip;
}

float
camera_get_far_clip(te_camera_component* comp) {
    return comp->far_clip;
}

unsigned char
camera_get_vertical_fov(struct te_camera_component* comp) {
    return comp->vertical_fov;
}

unsigned int
prv_camera_component_get_sizeof(void) {
    return sizeof(te_camera_component);
}

void
prv_camera_component_init(void* data) {
    te_camera_component* comp = data;

    comp->near_clip = 0.2f;
    comp->far_clip = 150.0f;
    comp->vertical_fov = 90;
    comp->is_properties_changed = true;
}

bool
prv_camera_is_properties_changed(struct te_camera_component* comp) {
    return comp->is_properties_changed;
}

void
prv_camera_reset_properties_changed_flag(struct te_camera_component* comp) {
    comp->is_properties_changed = false;
}
