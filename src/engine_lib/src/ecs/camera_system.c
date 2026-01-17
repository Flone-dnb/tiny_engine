#include "ecs/camera_system.h"

#include <stdlib.h>
#include "ecs/camera_component.h"
#include "ecs/ecs.h"
#include "ecs/transform_component.h"
#include "misc/error.h"

/** Manages camera components. */
typedef struct te_camera_system {
    /** Pointer to ECS manager. Do not delete/free this pointer. Valid while the system exists. */
    te_ecs* ecs;

    /** Pointer to the camera component of @ref entity_id. */
    struct te_camera_component* camera;

    /** Pointer to the transform component of @ref entity_id. */
    struct te_transform_component* transform;

    /** View matrix of @ref camera. May be outdated. */
    mat4 view;

    /** Projection matrix of @ref camera. May be outdated. */
    mat4 proj;

    /**
     * ID of an entity (with a camera component) which should be considered
     * the active camera to render the world.
     * TE_ECS_ENTITY_ID_INVALID if no active camera.
     */
    unsigned int entity_id;
} te_camera_system;

void
on_system_registered_collect_entities(void* user_system, te_ecs* ecs) {
    (void)user_system;
    (void)ecs;
    // Nothing to do here.
}

void
on_after_entity_created(void* user_system, te_ecs* ecs, unsigned int entity_id) {
    (void)user_system;
    (void)ecs;
    (void)entity_id;
    // Nothing to do here.
}

void
on_before_entity_destroyed(void* user_system, te_ecs* ecs, unsigned int entity_id) {
    (void)ecs;

    te_camera_system* sys = user_system;

    if (sys->entity_id == entity_id) {
        sys->entity_id = TE_ECS_ENTITY_ID_INVALID;
        sys->camera = NULL;
        sys->transform = NULL;
    }
}

te_camera_system*
prv_camera_system_create(struct te_ecs* ecs) {
    te_camera_system* sys = malloc(sizeof(te_camera_system));

    sys->ecs = ecs;
    sys->entity_id = TE_ECS_ENTITY_ID_INVALID;
    sys->camera = NULL;
    sys->transform = NULL;
    glm_mat4_identity(sys->view);
    glm_mat4_identity(sys->proj);

    ecs_register_system(ecs, sys, on_system_registered_collect_entities, on_after_entity_created,
                        on_before_entity_destroyed);

    return sys;
}

void
prv_camera_system_destroy(te_camera_system* system) {
    ecs_unregister_system(system->ecs, system);

    free(system);
}

void
prv_camera_system_update_camera_matrices(te_camera_system* sys) {
#if defined(DEBUG)
    // Self check:
    if (sys->entity_id == TE_ECS_ENTITY_ID_INVALID) {
        show_error_and_abort("this function should only be called while there is a valid active camera");
    }
#endif

    vec3 location;
    transform_get_location(sys->transform, location);

    glm_look_rh(location, TODO, TODO, sys->view);

    prv_camera_reset_properties_changed_flag(sys->camera);
}

bool
camera_system_is_camera_set(struct te_camera_system* system) {
    return system->entity_id != TE_ECS_ENTITY_ID_INVALID;
}

void
camera_system_get_camera_view(struct te_camera_system* system, mat4 out) {
#if defined(DEBUG)
    if (system->entity_id == TE_ECS_ENTITY_ID_INVALID) {
        show_error_and_abort("there is no active camera to query camera's properties");
    }
#endif

    if (prv_camera_is_properties_changed(system->camera)) {
        prv_camera_system_update_camera_matrices(system);
    }

    glm_mat4_copy(system->view, out);
}

void
camera_system_get_camera_proj(struct te_camera_system* system, mat4 out) {
#if defined(DEBUG)
    if (system->entity_id == TE_ECS_ENTITY_ID_INVALID) {
        show_error_and_abort("there is no active camera to query camera's properties");
    }
#endif

    if (prv_camera_is_properties_changed(system->camera)) {
        prv_camera_system_update_camera_matrices(system);
    }

    glm_mat4_copy(system->proj, out);
}

void
camera_system_set_active_camera(te_camera_system* system, unsigned int entity_id) {
    struct te_camera_component* camera =
        ecs_get_entity_component(system->ecs, entity_id, TE_ECS_COMPONENT_TYPE_CAMERA);
    struct te_transform_component* transform =
        ecs_get_entity_component(system->ecs, entity_id, TE_ECS_COMPONENT_TYPE_TRANSFORM);

    if (camera == NULL) {
        show_error_and_abort(
            "the specified entity cannot be used as a camera as it does not have the camera component");
    }
    if (transform == NULL) {
        show_error_and_abort(
            "the specified entity cannot be used as a camera as it does not have the transform component");
    }

    system->entity_id = entity_id;
    system->camera = camera;
    system->transform = transform;

    prv_camera_system_update_camera_matrices(system);
}
