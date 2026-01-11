#include "world.h"

#include <stdlib.h>
#include <string.h>

te_world*
prv_world_create(struct te_game_manager* game_manager, const char* name) {
    te_world* world = malloc(sizeof(te_world));

    world->game_manager = game_manager;

    // Copy name.
    const unsigned long name_len = strlen(name);
    world->name = malloc(sizeof(char) * name_len + 1);
    memcpy(world->name, name, name_len);
    world->name[name_len] = 0;

    return world;
}

void
prv_world_destroy(te_world* world) {
    free(world->name);

    free(world);
}

const char*
world_get_name(te_world* world) {
    return world->name;
}
