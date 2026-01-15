#include "world.h"

#include <stdlib.h>
#include <string.h>
#include "ecs/ecs.h"

/** World represents several objects: audio system, cameras, game objects and etc. */
struct te_world {
    /** Always valid pointer. Game manager that owns this world. You should not free/destroy this pointer. */
    struct te_game_manager* game_manager;

    /** ECS for this world. */
    struct te_ecs* ecs;

    /** World name. */
    char* name;
};

const char*
world_get_name(te_world* world) {
    return world->name;
}

te_world*
prv_world_create(struct te_game_manager* game_manager, const char* name) {
    te_world* world = malloc(sizeof(te_world));

    world->game_manager = game_manager;
    world->ecs = ecs_create();

    // Copy name.
    const unsigned long name_len = strlen(name);
    world->name = malloc(sizeof(char) * (name_len + 1));
    memcpy(world->name, name, name_len);
    world->name[name_len] = 0;

    return world;
}

void
prv_world_destroy(te_world* world) {
    ecs_destroy(world->ecs);
    free(world->name);

    free(world);
}

void
prv_world_tick(te_world* world, float delta_time_sec) {
    prv_ecs_tick(world->ecs, delta_time_sec);
}
