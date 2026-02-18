#pragma once

#include "cglm/vec2.h"

// Core component of any widget. This type handles hierarchy functionality.
// Widgets store (own) this object inside of them.
//
// Generally you should not create this object directly, other widgets like text or button
// will create this object themselves.
typedef struct te_widget te_widget;
struct te_world;

// Accepts a few callbacks that will be triggered when the widget is modified.
// "Pos/size changed" callback will be triggered in both cases: when relative or screen pos/size changes.
te_widget* widget_create(
    void* owner, void (*on_pos_changed)(void* owner), void (*on_size_changed)(void* owner),
    void (*on_before_base_destroyed)(void* owner), void (*on_after_spawned)(void* owner),
    void (*on_before_despawned)(void* owner), void (*on_window_size_changed)(void* owner));
void widget_destroy(te_widget* widget);

// Sets or changes the current parent of a widget.
// Specify NULL to remove parent.
//
// If a widget is being destroyed it will also destroy all child widgets.
void widget_set_parent(te_widget* widget, te_widget* new_parent);
te_widget* widget_get_parent(te_widget* widget);

// Sets position of the widget in range [0.0; 1.0] relative to the window's top-left corner.
// If the widget has a parent then this position becomes relative to the parent's position/size.
void widget_set_relative_position(te_widget* widget, vec2 position);
void widget_get_relative_position(te_widget* widget, vec2 out);

// Sets size of the widget in range [0.0; 1.0] relative to the window's top-left corner.
// If the widget has a parent then this size becomes relative to the parent's size.
void widget_set_relative_size(te_widget* widget, vec2 size);
void widget_get_relative_size(te_widget* widget, vec2 out);

// Returns widget's position and size in range [0.0; 1.0] relative to the window's size.
// Includes transformations of all parents (if the widget has parents).
void widget_get_screen_position(te_widget* widget, vec2 pos);
void widget_get_screen_size(te_widget* widget, vec2 size);

// Returns NULL if not spawned.
struct te_world* widget_get_world(te_widget* widget);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Called to notify the widget about being spawned/despawned.
// Recursively call this function on all child nodes.
void prv_widget_on_spawned(te_widget* widget, struct te_world* world);
void prv_widget_on_despawned(te_widget* widget);

// Recursively calls this function on all child widgets.
void prv_widget_on_window_size_changed(te_widget* widget);
