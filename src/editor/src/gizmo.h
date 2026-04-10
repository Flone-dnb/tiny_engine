#pragma once

#include <stdbool.h>

typedef struct te_gizmo te_gizmo;
struct te_world;
struct te_model;
struct te_camera;

// Creates and spawns gizmo model in the specified world to control the specified target object.
// Will be automatically destroyed when the model is despawned and destroyed.
te_gizmo* gizmo_create_in_world(struct te_world* world, struct te_model* target);

// Despawns and destroys the gizmo.
// Although the gizmo will be automatically despawned and destroyed upon world destruction
// you can call this function to do it right now.
void gizmo_destroy_in_world_now(te_gizmo* gizmo, struct te_world* world);

// Returns game object that the gizmo controls.
void* gizmo_get_target(te_gizmo* gizmo);

void gizmo_start_grab_x(te_gizmo* gizmo);
void gizmo_start_grab_y(te_gizmo* gizmo);
void gizmo_start_grab_z(te_gizmo* gizmo);

void gizmo_end_grab(te_gizmo* gizmo);

bool gizmo_is_grabbed(te_gizmo* gizmo);

// Moves the gizmo (and the controlled object) according to the specified mouse movement offset.
// Expects that the gizmo is grabbed.
void gizmo_move(te_gizmo* gizmo, struct te_camera* camera, float x_offset, float y_offset);

struct te_model* gizmo_get_model_x(te_gizmo* gizmo);
struct te_model* gizmo_get_model_y(te_gizmo* gizmo);
struct te_model* gizmo_get_model_z(te_gizmo* gizmo);