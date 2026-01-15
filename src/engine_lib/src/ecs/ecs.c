#include "ecs/ecs.h"

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "misc/error.h"

/** Types of callbacks the systems can register. */
enum te_ecs_callback_type : unsigned int {
    ECS_CALLBACK_TYPE_ON_TICK = 0,
    // ... new types go here ...

    ECS_CALLBACK_TYPE_COUNT, // <- mark the total number of elements in the enum
};

struct te_ecs_system;

/** Groups information about a system's registered callback. */
typedef struct te_ecs_callback_info_on_tick {
    /** Callback. */
    void (*callback)(void* system, struct te_ecs* ecs, float delta_time_sec);

    /** ECS registered system. */
    struct te_ecs_system* ecs_system;
} te_ecs_callback_info_on_tick;

/** Groups information about callbacks that the system registered. */
typedef struct te_ecs_registered_system_callback_info {
    /** Type of the callback. */
    enum te_ecs_callback_type callback_type;

    /** Index of the callback in the global array of callbacks (in the ECS manager). */
    unsigned int callback_index;
} te_ecs_registered_system_callback_info;

/** Registered system (in the ECS manager). */
typedef struct te_ecs_system {
    /** User system. Always valid. You should not free/destroy this pointer (managed by the user). */
    void* user_system;

    /** Callbacks registered by the system. */
    te_ecs_registered_system_callback_info* registered_callbacks;

    /**
     * Callback that will be called after some new entity was created for the
     * system to check if it needs to consider it or not.
     */
    void (*on_after_entity_created)(void* user_system, struct te_ecs* ecs, unsigned int entity_id);

    /**
     * Callback that will be called before an entity is destroyed for the 
     * system to possibly remove it from system's list of considered entities.
     */
    void (*on_before_entity_destroyed)(void* user_system, struct te_ecs* ecs, unsigned int entity_id);

    /** The number of elements in the array @ref registered_callbacks. */
    unsigned int registered_callback_count;
} te_ecs_system;

/** Groups info about a registered component type. */
typedef struct te_ecs_component_type_info {
    /** Callback that must be called after the component's memory was allocated. */
    void (*init_component)(void* component_data);

    /** Size of the component in bytes. */
    unsigned int size_in_bytes;
} te_ecs_component_type_info;

/** Information about a component of some entity. */
typedef struct te_ecs_entity_component {
    /** Type ID of the component. */
    unsigned int component_type_id;

    /** Index to the component data in the global array of components. */
    unsigned int component_index;
} te_ecs_entity_component;

/** Groups information about a registered entity. */
typedef struct te_ecs_entity {
    /** Name of the entity. NULL if the entity data is invalid (unused). */
    char* name;

    /** Components of the entity. Size of this array is @ref component_count. */
    te_ecs_entity_component* components;

    /** Number of elements in the array @ref components. */
    unsigned int component_count;

    /** @ref TE_ECS_ENTITY_ID_INVALID if no parent entity. */
    unsigned int opt_parent_entity_id;
} te_ecs_entity;

/** Groups information about components of the same type. */
typedef struct te_ecs_components_of_type {
    /**
     * Array of components (data) of the same type. Can have "gaps" of invalid data see
     * @ref unused_components.
     */
    void* components;

    /** Indices of no longer used components in @ref components. */
    unsigned int* unused_component_indices;

    /** Number of elements in @ref components that have valid data. */
    unsigned int valid_component_count;

    /** Total number of elements that @ref components can have. */
    unsigned int component_array_size;

    /** Number of elements in the array @ref unused_component_indices. */
    unsigned int unused_component_count;
} te_ecs_components_of_type;

/** Manager for entities, components and systems. */
struct te_ecs {
    /** All currently registered systems. */
    te_ecs_system** systems;

    /**
     * Components of all registered entities.
     * Size of this array is @ref component_type_count.
     */
    te_ecs_components_of_type* components_by_type;

    /** Registered "on tick" callbacks for registered systems. Size of this array is @ref callback_count_on_tick. */
    te_ecs_callback_info_on_tick* callbacks_on_tick;

    /** Registered component types. Size of this array is @ref component_type_count. */
    te_ecs_component_type_info* component_types;

    /**
     * Registered entities. Size of this array is @ref entity_array_size while the number of
     * valid (used) items in this array is @ref registered_entity_count.
     * This array does not shrink, it only grows. @ref unused_entity_indices
     * array stores indices into this array where entity data is no longer used and can be reused.
     * So this array can have "gaps" where the data is invalid.
     */
    te_ecs_entity* entities;

    /**
     * Array of indices into @ref entities where entity data can be reused.
     * Size of this array is @ref unused_entity_count.
     */
    unsigned int* unused_entity_indices;

    /** Number of elements in the array @ref registered_systems. */
    unsigned int system_count;

    /** Number of elements in the array @ref callbacks_on_tick. */
    unsigned int callback_count_on_tick;

    /**
     * Number of elements in the array @ref component_types.
     * Because component types can't be unregistered this value is never decremented.
     */
    unsigned int component_type_count;

    /** Number of registered (used) entities in the array @ref entities. */
    unsigned int registered_entity_count;

    /** Number of elements in the array @ref entities. */
    unsigned int entity_array_size;

    /** Number of elements in the array @ref unused_entity_indices. */
    unsigned int unused_entity_count;

    /** `true` when running a loop in which we call registered system callbacks. */
    bool is_triggering_system_callbacks[ECS_CALLBACK_TYPE_COUNT];

    /** `true` if something registered/unregistered while @ref is_triggering_system_callbacks was `true`. */
    bool is_callbacks_changed_while_triggering[ECS_CALLBACK_TYPE_COUNT];

    /** `true` if creating or destroying an entity. */
    bool is_creating_or_destroying_entity;
};

te_ecs*
ecs_create(void) {
    te_ecs* ecs = malloc(sizeof(te_ecs));

    ecs->system_count = 0;
    ecs->systems = NULL;

    ecs->callback_count_on_tick = 0;
    ecs->callbacks_on_tick = NULL;

    ecs->component_types = NULL;
    ecs->component_type_count = 0;

    ecs->entities = NULL;
    ecs->registered_entity_count = 0;
    ecs->entity_array_size = 0;

    ecs->unused_entity_indices = NULL;
    ecs->unused_entity_count = 0;

    ecs->components_by_type = NULL;

    for (unsigned int i = 0; i < ECS_CALLBACK_TYPE_COUNT; i++) {
        ecs->is_triggering_system_callbacks[i] = false;
        ecs->is_callbacks_changed_while_triggering[i] = false;
    }
    ecs->is_creating_or_destroying_entity = false;

    return ecs;
}

void
ecs_destroy(te_ecs* ecs) {
    if (ecs->system_count > 0) {
        show_error_and_abort("ECS is being destroyed but there are still some systems registered");
    }
    if (ecs->callback_count_on_tick > 0) {
        show_error_and_abort("ECS is being destroyed but there are still some callbacks registered");
    }

    if (ecs->registered_entity_count > 0) {
        show_error_and_abort("ECS is being destroyed but there are still some entities registered");
    }
    free(ecs->entities);
    ecs->entity_array_size = 0;

    for (unsigned int i = 0; i < ecs->component_type_count; i++) {
        free(ecs->components_by_type[i].components);
        free(ecs->components_by_type[i].unused_component_indices);
        ecs->components_by_type[i].valid_component_count = 0;
        ecs->components_by_type[i].component_array_size = 0;
        ecs->components_by_type[i].unused_component_count = 0;
    }
    free(ecs->components_by_type);
    ecs->components_by_type = NULL;

    free(ecs->unused_entity_indices);
    ecs->unused_entity_count = 0;

    free(ecs->component_types);
    ecs->component_type_count = 0;

    free(ecs);
}

void
ecs_register_component_type(te_ecs* ecs, unsigned int type_id, unsigned int size_in_bytes,
                            void (*init_component)(void* component_data)) {
    if (ecs->component_type_count == UINT_MAX) {
        show_error_and_abort("reached type limit for registered component types");
    }

    if (type_id != ecs->component_type_count) {
        show_error_and_abort("you must register component type ids in order: 0, 1, 2, 3 not 0, 2, 1, 3");
    }

    // Expand the array of component types.
    {
        te_ecs_component_type_info* new_types =
            malloc(sizeof(te_ecs_component_type_info) * (ecs->component_type_count + 1));

        memcpy(new_types, ecs->component_types,
               sizeof(te_ecs_component_type_info) * ecs->component_type_count);

        free(ecs->component_types);
        ecs->component_types = new_types;
    }

    // Expand the array of component data by type.
    {
        te_ecs_components_of_type* new_components =
            malloc(sizeof(te_ecs_components_of_type) * (ecs->component_type_count + 1));

        memcpy(new_components, ecs->components_by_type, ecs->component_type_count);

        free(ecs->components_by_type);
        ecs->components_by_type = new_components;
    }

    ecs->component_types[ecs->component_type_count].size_in_bytes = size_in_bytes;
    ecs->component_types[ecs->component_type_count].init_component = init_component;

    ecs->component_type_count += 1;
}

void
ecs_register_system(te_ecs* ecs, void* user_system,
                    void (*on_system_registered_collect_entities)(void* user_system, te_ecs* ecs),
                    void (*on_after_entity_created)(void* user_system, te_ecs* ecs, unsigned int entity_id),
                    void (*on_before_entity_destroyed)(void* user_system, te_ecs* ecs,
                                                       unsigned int entity_id)) {
    if (ecs->is_creating_or_destroying_entity) {
        // Just for simplicity.
        show_error_and_abort("systems are not allowed to register/unregister in entity create/destroy "
                             "callbacks, this is allowed in other callbacks");
    }

    // Make sure we don't hit type limit.
    if (ecs->system_count == UINT_MAX) {
        show_error_and_abort("reached the maximum number of registered systems");
    }

    // Expand array.
    {
        te_ecs_system** new_systems = malloc(sizeof(te_ecs_system*) * (ecs->system_count + 1));

        memcpy(new_systems, ecs->systems, sizeof(te_ecs_system*) * ecs->system_count);

        free(ecs->systems);
        ecs->systems = new_systems;
    }

    // Create new system.
    te_ecs_system* sys = malloc(sizeof(te_ecs_system));
    sys->user_system = user_system;
    sys->on_after_entity_created = on_after_entity_created;
    sys->on_before_entity_destroyed = on_before_entity_destroyed;
    sys->registered_callbacks = NULL;
    sys->registered_callback_count = 0;

    ecs->systems[ecs->system_count] = sys;
    ecs->system_count += 1;

    // Notify system.
    on_system_registered_collect_entities(system, ecs);
}

void
ecs_unregister_system(te_ecs* ecs, void* user_system) {
    if (ecs->is_creating_or_destroying_entity) {
        // Just for simplicity.
        show_error_and_abort("systems are not allowed to register/unregister in entity create/destroy "
                             "callbacks, this is allowed in other callbacks");
    }

    // Find the specified system (and make sure it was registered).
    unsigned int system_index = 0;
    bool found_system = false;
    for (unsigned int i = 0; i < ecs->system_count; i++) {
        if (ecs->systems[i]->user_system == user_system) {
            system_index = i;
            found_system = true;
            break;
        }
    }
    if (!found_system) {
        show_error_and_abort("the specified system is not registered");
    }

    // Destroy the system.
    if (ecs->systems[system_index]->registered_callback_count > 0) {
        show_error_and_abort("the system is being unregistered but it still has some "
                             "callback not unregistered");
    }
    free(ecs->systems[system_index]);

    // Shrink the systems array.
    if (ecs->system_count == 1) {
        free(ecs->systems);
        ecs->systems = NULL;
    } else {
        te_ecs_system** new_systems = malloc(sizeof(te_ecs_system*) * (ecs->system_count - 1));

        memcpy(new_systems, ecs->systems, sizeof(te_ecs_system*) * system_index);
        memcpy(new_systems + system_index, ecs->systems + (system_index + 1),
               sizeof(te_ecs_system*) * (ecs->system_count - system_index - 1));

        free(ecs->systems);
        ecs->systems = new_systems;
    }

    ecs->system_count -= 1;
}

unsigned int
ecs_create_entity(te_ecs* ecs, const char* name, unsigned int component_count,
                  unsigned int* component_type_ids, unsigned int opt_parent_entity_id) {
    if (ecs->registered_entity_count == TE_ECS_ENTITY_ID_INVALID) {
        show_error_and_abort("reached the maximum number of entities");
    }
    if (name == NULL) {
        show_error_and_abort("entity's name pointer must not be NULL");
    }
    if (component_type_ids == NULL) {
        show_error_and_abort("entity's component type IDs pointer must not be NULL");
    }

    unsigned int entity_index = 0;

    if (ecs->unused_entity_count > 0) {
        // Take unused slot.
        entity_index = ecs->unused_entity_indices[0];

        // Shrink the array.
        ecs->unused_entity_count -= 1;
        if (ecs->unused_entity_count == 0) {
            free(ecs->unused_entity_indices);
            ecs->unused_entity_indices = NULL;
        } else {
            unsigned int* new_indices = malloc(sizeof(unsigned int) * ecs->unused_entity_count);

            memcpy(new_indices, ecs->unused_entity_indices + 1,
                   sizeof(unsigned int) * ecs->unused_entity_count);

            free(ecs->unused_entity_indices);
            ecs->unused_entity_indices = new_indices;
        }
    } else {
        // Check if we need to expand the entity array.
        if (ecs->registered_entity_count == ecs->entity_array_size) {
            // Expand (preallocate more space than needed).
            const unsigned int expand_size = 128;
            te_ecs_entity* new_entities =
                malloc(sizeof(te_ecs_entity) * (ecs->registered_entity_count + expand_size));

            memcpy(new_entities, ecs->entities, sizeof(te_ecs_entity) * ecs->registered_entity_count);

            free(ecs->entities);
            ecs->entities = new_entities;

            ecs->entity_array_size += expand_size;
        }

        entity_index = ecs->registered_entity_count;
        ecs->registered_entity_count += 1;
    }

    // Init entity.
    te_ecs_entity* entity = &ecs->entities[entity_index];

    const unsigned long name_len = strlen(name);
    entity->name = malloc(sizeof(char) * (name_len + 1));
    memcpy(entity->name, name, name_len);
    entity->name[name_len] = 0;

    entity->component_count = component_count;
    entity->opt_parent_entity_id = opt_parent_entity_id;

    // Init components.
    entity->components = malloc(sizeof(te_ecs_entity_component) * component_count);
    for (unsigned int i = 0; i < component_count; i++) {
        if (component_type_ids[i] >= ecs->component_type_count) {
            show_error_and_abort("invalid component type ID specified");
        }

        const unsigned int component_type_size = ecs->component_types[component_type_ids[i]].size_in_bytes;

        // Find an index to store the component's data.
        unsigned int component_index = 0;
        te_ecs_components_of_type* comps_data = &ecs->components_by_type[component_type_ids[i]];
        if (comps_data->unused_component_count > 0) {
            // Take unused slot.
            component_index = comps_data->unused_component_indices[0];

            // Shrink the array.
            comps_data->unused_component_count -= 1;
            if (comps_data->unused_component_count == 0) {
                free(comps_data->unused_component_indices);
                comps_data->unused_component_indices = NULL;
            } else {
                unsigned int* new_indices = malloc(sizeof(unsigned int) * comps_data->unused_component_count);

                memcpy(new_indices, comps_data->unused_component_indices + 1,
                       sizeof(unsigned int) * comps_data->unused_component_count);

                free(comps_data->unused_component_indices);
                comps_data->unused_component_indices = new_indices;
            }
        } else {
            // Check if we need to expand the array.
            if (comps_data->valid_component_count == comps_data->component_array_size) {
                // Expand (preallocate more space than needed).
                const unsigned int expand_size = 8;
                void* new_components =
                    malloc(component_type_size * (comps_data->valid_component_count + expand_size));

                memcpy(new_components, comps_data->components,
                       component_type_size * comps_data->valid_component_count);

                free(comps_data->components);
                comps_data->components = new_components;

                comps_data->component_array_size += expand_size;
            }

            component_index = comps_data->valid_component_count;
            comps_data->valid_component_count += 1;
        }

        // Init component data.
        ecs->component_types[component_type_ids[i]].init_component(&comps_data[component_index]);

        // Assign to the entity.
        entity->components[i].component_type_id = component_type_ids[i];
        entity->components[i].component_index = component_index;
    }

    // Notify all systems.
    {
        ecs->is_creating_or_destroying_entity = true;
        for (unsigned int i = 0; i < ecs->system_count; i++) {
            ecs->systems[i]->on_after_entity_created(ecs->systems[i]->user_system, ecs, entity_index);
        }
        ecs->is_creating_or_destroying_entity = false;
    }

    return entity_index;
}

void
ecs_destroy_entity(te_ecs* ecs, unsigned int entity_id) {
    if (entity_id >= ecs->entity_array_size) {
        show_error_and_abort("invalid entity id specified");
    }

    // Notify all systems.
    {
        ecs->is_creating_or_destroying_entity = true;
        for (unsigned int i = 0; i < ecs->system_count; i++) {
            ecs->systems[i]->on_before_entity_destroyed(ecs->systems[i]->user_system, ecs, entity_id);
        }
        ecs->is_creating_or_destroying_entity = false;
    }

    te_ecs_entity* entity = &ecs->entities[entity_id];

    // First, destroy components.
    for (unsigned int i = 0; i < entity->component_count; i++) {
        te_ecs_entity_component* comp_info = &entity->components[i];
        te_ecs_components_of_type* comps_data = &ecs->components_by_type[comp_info->component_type_id];

        comps_data->valid_component_count -= 1;

        // Add index to unused.
        unsigned int* new_unused = malloc(sizeof(unsigned int) * (comps_data->unused_component_count + 1));

        memcpy(new_unused, comps_data->unused_component_indices,
               sizeof(unsigned int) * comps_data->unused_component_count);

        free(comps_data->unused_component_indices);
        comps_data->unused_component_indices = new_unused;

        comps_data->unused_component_indices[comps_data->unused_component_count] = comp_info->component_index;
        comps_data->unused_component_count += 1;
    }
    free(entity->components);
    entity->components = NULL;
    entity->component_count = 0;

    // Second, destroy the entity.
    free(entity->name);
    entity->name = NULL;

    // Add to unused.
    {
        ecs->registered_entity_count -= 1;

        unsigned int* new_unused = malloc(sizeof(unsigned int) * (ecs->unused_entity_count + 1));

        memcpy(new_unused, ecs->unused_entity_indices, sizeof(unsigned int) * ecs->unused_entity_count);

        free(ecs->unused_entity_indices);
        ecs->unused_entity_indices = new_unused;

        ecs->unused_entity_indices[ecs->unused_entity_count] = entity_id;
        ecs->unused_entity_count += 1;
    }

    if (entity->opt_parent_entity_id != TE_ECS_ENTITY_ID_INVALID) {
        const unsigned int parent_entity_id = entity->opt_parent_entity_id;
        entity->opt_parent_entity_id = TE_ECS_ENTITY_ID_INVALID;

        ecs_destroy_entity(ecs, parent_entity_id);
    }
}

void
ecs_system_register_callback_on_tick(te_ecs* ecs, void* user_system,
                                     void (*on_tick)(void* system, te_ecs* ecs, float delta_time_sec)) {
    // Find the specified system (and make sure it was registered).
    unsigned int system_index = 0;
    bool found_system = false;
    for (unsigned int i = 0; i < ecs->system_count; i++) {
        if (ecs->systems[i]->user_system == user_system) {
            system_index = i;
            found_system = true;
            break;
        }
    }
    if (!found_system) {
        show_error_and_abort("the specified system is not registered");
    }
    te_ecs_system* sys = ecs->systems[system_index];

    unsigned int callback_index = 0;

    // Update system's callback info array.
    {
        // Expand global array of callbacks.
        te_ecs_callback_info_on_tick* new_callbacks =
            malloc(sizeof(te_ecs_callback_info_on_tick) * (ecs->callback_count_on_tick + 1));
        memcpy(new_callbacks, ecs->callbacks_on_tick,
               sizeof(te_ecs_callback_info_on_tick) * ecs->callback_count_on_tick);

        // Put new callback.
        new_callbacks[ecs->callback_count_on_tick].callback = on_tick;
        new_callbacks[ecs->callback_count_on_tick].ecs_system = sys;
        callback_index = ecs->callback_count_on_tick;
        ecs->callback_count_on_tick += 1;

        free(ecs->callbacks_on_tick);
        ecs->callbacks_on_tick = new_callbacks;
    }

    // Update global callback array.
    {
        // Expand array of system callback infos.
        te_ecs_registered_system_callback_info* new_callbacks =
            malloc(sizeof(te_ecs_registered_system_callback_info) * (sys->registered_callback_count + 1));
        memcpy(new_callbacks, sys->registered_callbacks,
               sizeof(te_ecs_registered_system_callback_info) * sys->registered_callback_count);

        // Add new info.
        new_callbacks[sys->registered_callback_count].callback_type = ECS_CALLBACK_TYPE_ON_TICK;
        new_callbacks[sys->registered_callback_count].callback_index = callback_index;
        sys->registered_callback_count += 1;

        free(sys->registered_callbacks);
        sys->registered_callbacks = new_callbacks;
    }

    ecs->is_callbacks_changed_while_triggering[ECS_CALLBACK_TYPE_ON_TICK] =
        ecs->is_triggering_system_callbacks[ECS_CALLBACK_TYPE_ON_TICK];
}

void
ecs_system_unregister_callback_on_tick(te_ecs* ecs, void* user_system) {
    // Find the specified system (and make sure it was registered).
    unsigned int system_index = 0;
    bool found_system = false;
    for (unsigned int i = 0; i < ecs->system_count; i++) {
        if (ecs->systems[i]->user_system == user_system) {
            system_index = i;
            found_system = true;
            break;
        }
    }
    if (!found_system) {
        show_error_and_abort("the specified system is not registered");
    }
    te_ecs_system* sys = ecs->systems[system_index];

    unsigned int global_callback_index = 0;

    // Update system's callback info array.
    {
        // Find callback index.
        bool found_callback = false;
        unsigned int callback_info_index = 0;
        for (unsigned int i = 0; i < sys->registered_callback_count; i++) {
            if (sys->registered_callbacks[i].callback_type == ECS_CALLBACK_TYPE_ON_TICK) {
                found_callback = true;
                global_callback_index = sys->registered_callbacks[i].callback_index;
                callback_info_index = i;
                break;
            }
        }
        if (!found_callback) {
            show_error_and_abort("the specified system have not registered this callback previously");
        }

        // Shrink system's callbacks info array.
        if (sys->registered_callback_count == 1) {
            free(sys->registered_callbacks);
            sys->registered_callbacks = NULL;
        } else {
            te_ecs_registered_system_callback_info* new_callbacks =
                malloc(sizeof(te_ecs_registered_system_callback_info) * (sys->registered_callback_count - 1));

            memcpy(new_callbacks, sys->registered_callbacks,
                   sizeof(te_ecs_registered_system_callback_info) * callback_info_index);
            memcpy(new_callbacks + callback_info_index, sys->registered_callbacks + (callback_info_index + 1),
                   sizeof(te_ecs_registered_system_callback_info)
                       * (sys->registered_callback_count - callback_info_index - 1));

            free(sys->registered_callbacks);
            sys->registered_callbacks = new_callbacks;
        }

        sys->registered_callback_count -= 1;
    }

    // Update global callback array.
    {
        if (global_callback_index >= ecs->callback_count_on_tick) {
            show_error_and_abort("system stored invalid index into the global array of "
                                 "callbacks or global "
                                 "callback counter was invalid");
        }

        // Shrink callbacks array.
        if (ecs->callback_count_on_tick == 1) {
            free(ecs->callbacks_on_tick);
            ecs->callbacks_on_tick = NULL;
        } else {
            te_ecs_callback_info_on_tick* new_callbacks =
                malloc(sizeof(te_ecs_callback_info_on_tick) * (ecs->callback_count_on_tick - 1));

            memcpy(new_callbacks, ecs->callbacks_on_tick,
                   sizeof(te_ecs_callback_info_on_tick) * global_callback_index);
            memcpy(new_callbacks + global_callback_index,
                   ecs->callbacks_on_tick + (global_callback_index + 1),
                   sizeof(te_ecs_callback_info_on_tick)
                       * (ecs->callback_count_on_tick - global_callback_index - 1));

            // And also update indices in other systems that we shifted to the left (in the array).
            for (unsigned int i = global_callback_index + 1; i < ecs->callback_count_on_tick; i++) {
                te_ecs_system* sys_to_update = ecs->callbacks_on_tick[i].ecs_system;
                for (unsigned int j = 0; j < sys_to_update->registered_callback_count; j++) {
                    te_ecs_registered_system_callback_info* info = &sys_to_update->registered_callbacks[j];
                    if (info->callback_type == ECS_CALLBACK_TYPE_ON_TICK) {
                        info->callback_index -= 1;
                        break;
                    }
                }
            }

            free(ecs->callbacks_on_tick);
            ecs->callbacks_on_tick = new_callbacks;
        }

        ecs->callback_count_on_tick -= 1;
    }

    ecs->is_callbacks_changed_while_triggering[ECS_CALLBACK_TYPE_ON_TICK] =
        ecs->is_triggering_system_callbacks[ECS_CALLBACK_TYPE_ON_TICK];
}

unsigned int*
ecs_get_entity_ids(te_ecs* ecs, unsigned int* count) {
    *count = ecs->registered_entity_count;
    if (ecs->registered_entity_count == 0) {
        return NULL;
    }

    unsigned int* entity_ids = malloc(sizeof(unsigned int) * ecs->registered_entity_count);

    unsigned int current_entity_index = 0;
    for (unsigned int i = 0; i < ecs->registered_entity_count; i++) {
        if (ecs->entities[i].name == NULL) {
            continue;
        }

        entity_ids[current_entity_index] = i;
        current_entity_index += 1;
    }

    // Self check:
    if (current_entity_index != ecs->registered_entity_count) {
        show_error_and_abort("registered entity count is invalid");
    }

    return entity_ids;
}

void*
ecs_get_entity_component(te_ecs* ecs, unsigned int entity_id, unsigned int component_type_id) {
    if (entity_id >= ecs->registered_entity_count) {
        show_error_and_abort("invalid entity ID specified");
    }

    te_ecs_entity* entity = &ecs->entities[entity_id];
    for (unsigned int i = 0; i < entity->component_count; i++) {
        if (entity->components[i].component_type_id != component_type_id) {
            continue;
        }

        te_ecs_components_of_type* comps_data = &ecs->components_by_type[component_type_id];
        return &comps_data->components[entity->components[i].component_index];
    }

    return NULL;
}

void
prv_ecs_tick(te_ecs* ecs, float delta_time_sec) {
    ecs->is_triggering_system_callbacks[ECS_CALLBACK_TYPE_ON_TICK] = true;

    // Because a system (inside of its callback) can do various things such as unregistering itself
    // or triggering something that will make all systems unregister
    // it will create an issue where we in the middle of the for loop might accidentally skip some callbacks
    // (because the total number of callbacks changed and the pointer value also changed and etc.)
    // due to this we keep track of which callbacks we called and if something changed we recheck
    // all registered callbacks and make sure we haven't missed anything.
    // We also don't want to delay/queue registration or unregistration (to avoid doing these checks) because
    // such approach also has its issues.

    te_ecs_system** notified_systems = malloc(sizeof(te_ecs_system*) * ecs->callback_count_on_tick);
    unsigned int notified_system_count = 0;

    // We recheck all callbacks in case something re/unregistered.
    while (true) {
        bool check_if_already_notified = false;

        if (ecs->is_callbacks_changed_while_triggering[ECS_CALLBACK_TYPE_ON_TICK]) {
            // Make sure we will be able to fit all notified systems.
            te_ecs_system** new_notified_systems =
                malloc(sizeof(te_ecs_system*) * (notified_system_count + ecs->callback_count_on_tick));
            memcpy(new_notified_systems, notified_systems, notified_system_count);

            free(notified_systems);
            notified_systems = new_notified_systems;

            // Finished restarting.
            ecs->is_callbacks_changed_while_triggering[ECS_CALLBACK_TYPE_ON_TICK] = false;
            check_if_already_notified = true;
        }

        for (unsigned int i = 0; i < ecs->callback_count_on_tick; i++) {
            if (check_if_already_notified) {
                // Check if we already called this callback.
                bool already_notified = false;
                for (unsigned int system_i = 0; system_i < notified_system_count; system_i++) {
                    if (notified_systems[system_i] == ecs->callbacks_on_tick[i].ecs_system) {
                        already_notified = true;
                        break;
                    }
                }
                if (already_notified) {
                    continue;
                }
            }

            // Trigger user logic.
            ecs->callbacks_on_tick[i].callback(ecs->callbacks_on_tick[i].ecs_system->user_system, ecs,
                                               delta_time_sec);

            // Add system as notified.
            notified_systems[notified_system_count] = ecs->callbacks_on_tick[i].ecs_system;
            notified_system_count += 1;

            if (ecs->is_callbacks_changed_while_triggering[ECS_CALLBACK_TYPE_ON_TICK]) {
                // User logic changed something. Restart.
                break;
            }
        }

        if (ecs->is_callbacks_changed_while_triggering[ECS_CALLBACK_TYPE_ON_TICK]) {
            continue;
        }

        break;
    }

    free(notified_systems);

    ecs->is_triggering_system_callbacks[ECS_CALLBACK_TYPE_ON_TICK] = false;
}
