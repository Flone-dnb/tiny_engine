#pragma once

/** Manager for entities, components and systems. */
typedef struct te_ecs te_ecs;

/** Value used to specify "no entity ID". */
#define TE_ECS_ENTITY_ID_INVALID 0xffffffff

/** Engine component type IDs. */
enum te_ecs_engine_component_type_id : unsigned int {
    ECS_COMPONENT_TYPE_TRANSFORM = 0,
    // ... new engine component types go here ...

    ECS_COMPONENT_TYPE_FIRST_GAME_COMPONENT, // <- first component type ID for games
};

/**
 * Creates a new ECS manager.
 *
 * @return Created ECS manager.
 */
te_ecs* ecs_create(void);

/**
 * Destroys ECS manager.
 *
 * @param ecs ECS manager.
 */
void ecs_destroy(te_ecs* ecs);

/**
 * Registers a new type of a component to be attached to various entities.
 *
 * @remark Note that there's no unregister function for this.
 *
 * @param ecs ECS manager.
 *
 * @param type_id Unique ID of the component type. It's recommended to store IDs of your custom components
 * in an enum that starts with the ID @ref ECS_COMPONENT_TYPE_FIRST_GAME_COMPONENT to avoid ID conflicts
 * with the engine components.
 *
 * @param size_in_bytes Size of the component in bytes.
 * 
 * @param init_component Callback that must be specified, it will be called after a new component of this type is created
 * to initialize component data.
 */
void ecs_register_component_type(te_ecs* ecs, unsigned int type_id, unsigned int size_in_bytes,
                                 void (*init_component)(void* component_data));

/**
 * Registers a new system that will operate on entities and components.
 *
 * @remark You must use @ref ecs_unregister_system later.
 *
 * @param ecs ECS manager.
 *
 * @param user_system Pointer to the system (user object). Used to notify and differentiate systems.
 * Must be valid until the system is not unregistered.
 *
 * @param on_system_registered_collect_entities Callback that will be called after the system is
 * registered to collect/filter entities that it needs to operate on.
 *
 * @param on_after_entity_created Callback that will be called after some new entity was created for the
 * system to check if it needs to consider it or not.
 *
 * @param on_before_entity_destroyed Callback that will be called before an entity is destroyed for the
 * system to possibly remove it from system's list of considered entities.
 */
void ecs_register_system(te_ecs* ecs, void* user_system,
                         void (*on_system_registered_collect_entities)(void* user_system, te_ecs* ecs),
                         void (*on_after_entity_created)(void* user_system, te_ecs* ecs,
                                                         unsigned int entity_id),
                         void (*on_before_entity_destroyed)(void* user_system, te_ecs* ecs,
                                                            unsigned int entity_id));

/**
 * Unregisters a previously registered (using @ref ecs_register_system) system.
 *
 * @param ecs    ECS manager.
 * @param user_system User system to unregister.
 */
void ecs_unregister_system(te_ecs* ecs, void* user_system);

/**
 * Creates a new entity.
 *
 * @remark You must use @ref ecs_destroy_entity later.
 *
 * @param ecs ECS manager.
 *
 * @param name Name of the entity (must be non NULL). The string will be copied to the entity's internal data object.
 *
 * @param component_count Total number of components that the entity has.
 *
 * @param component_type_ids Non-NULL pointer to type IDs of the components that the entity has
 * (component types must be previously registered using @ref ecs_register_component_type).
 *
 * @param opt_parent_entity_id Specify TE_ECS_ENTITY_ID_INVALID if the newly entity is not a child of some other entity,
 * otherwise specify a valid ID of an existing entity.
 *
 * @return Unique ID of the created entity.
 */
unsigned int ecs_create_entity(te_ecs* ecs, const char* name, unsigned int component_count,
                               unsigned int* component_type_ids, unsigned int opt_parent_entity_id);

/**
 * Destroys an entity that was previously created using @ref ecs_create_entity.
 * Note that if this entity has child entities all child entities will also be destroyed (recursively).
 *
 * @param ecs ECS manager.
 *
 * @param entity_id Unique ID of the entity.
 */
void ecs_destroy_entity(te_ecs* ecs, unsigned int entity_id);

/**
 * Registers a callback that will be called every frame for the specified system.
 *
 * @remark You must use @ref ecs_system_unregister_callback_on_tick later.
 *
 * @param ecs ECS manager.
 * @param user_system Registered user system.
 * @param on_tick Callback that will be called.
 */
void ecs_system_register_callback_on_tick(te_ecs* ecs, void* user_system,
                                          void (*on_tick)(void* user_system, te_ecs* ecs,
                                                          float delta_time_sec));

/**
 * Unregisters a callback.
 *
 * @param ecs ECS manager.
 * @param user_system Registered user system.
 */
void ecs_system_unregister_callback_on_tick(te_ecs* ecs, void* user_system);

/**
 * Returns IDs of all registered entities.
 *
 * @param ecs ECS manager.
 *
 * @param count Non-NULL pointer to a variable that will mark the size of the returned array.
 *
 * @return NULL if 0 registered entities, otherwise an array of entity IDs, you must free this pointer after using it.
 */
unsigned int* ecs_get_entity_ids(te_ecs* ecs, unsigned int* count);

/**
 * Checks all components of the entity for a component of the specified type and if found returns it.
 *
 * @param ecs ECS manager.
 *
 * @param entity_id ID of a registered entity to check.
 *
 * @param component_type_id Type ID of a registered component.
 *
 * @return NULL if the specified entity does not have the specified component, otherwise pointer to the component's data
 * (you must cast the pointer type). Do not free/delete returned pointer, the pointer is valid until the entity is not destroyed.
 */
void* ecs_get_entity_component(te_ecs* ecs, unsigned int entity_id, unsigned int component_type_id);

/**
 * Returns a non-NULL pointer to the name of the entity.
 *
 * @param ecs ECS manager.
 *
 * @param entity_id ID of the entity.
 *
 * @return Non-NULL pointer to the name. You must not free/destroy returned pointer. The pointer
 * is valid until the entity is not destroyed.
 */
const char* ecs_get_entity_name(te_ecs* ecs, unsigned int entity_id);

/**
 * Returns TE_ECS_ENTITY_ID_INVALID if the entity does not have a parent entity, otherwise 
 * returns entity ID of the parent entity.
 *
 * @param ecs ECS manager.
 *
 * @param entity_id ID of the entity.
 *
 * @return TE_ECS_ENTITY_ID_INVALID if no parent.
 */
unsigned int ecs_get_entity_parent(te_ecs* ecs, unsigned int entity_id);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

/**
 * Called to trigger "on tick" callback on systems that registered it.
 *
 * @param ecs ECS manager.
 * @param delta_time_sec Time since the previous call to this function.
 */
void prv_ecs_tick(te_ecs* ecs, float delta_time_sec);
