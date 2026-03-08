#pragma once

typedef struct te_world_inspector te_world_inspector;
struct te_widget;

te_world_inspector* world_inspector_create(void);
void world_inspector_destroy(te_world_inspector* inspector);

void world_inspector_add(te_world_inspector* inspector, struct te_widget* left_panel);
void world_inspector_refresh(te_world_inspector* inspector);
