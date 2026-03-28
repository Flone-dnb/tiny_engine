#pragma once

#include <stdbool.h>
#include <cglm/vec2.h>
#include <input/keyboard_button.h>
#include <input/mouse_button.h>

// Core component of any widget. This type handles hierarchy functionality.
// Widgets store (own) this object inside of them.
//
// Generally you should not create this object directly, other widgets like text or button
// will create this object themselves.
typedef struct te_widget te_widget;
struct te_world;

// Callbacks that will be triggered when the widget is modified. Some may be specified as NULL.
// All callbacks specified here will only be called while the widget is spawned except for the "on before base destroyed" callback.
// "pos/size changed" callbacks will be triggered in both cases: when relative or screen pos/size changes.
// "on after spawned" callback is called before any child widget is spawned.
// "on before despawned" callback is called after all child widgets are despawned.
te_widget* widget_create(
    void* owner, const char* (*get_type_id)(void), void (*on_pos_changed)(void* owner),
    void (*on_size_changed)(void* owner), void (*on_before_base_destroyed)(void* owner),
    void (*on_parent_changed)(void* owner), void (*on_children_changed)(void* owner),
    void (*on_after_spawned)(void* owner), void (*on_before_despawned)(void* owner),
    void (*on_window_size_changed)(void* owner));
void widget_destroy(te_widget* widget);

// Returns the actual widget object that owns this base widget.
void* widget_get_owner(te_widget* widget);
const char* widget_get_owner_type_id(te_widget* widget);

// Used to add a custom (user-defined) value to the widget.
void widget_set_custom_value(te_widget* widget, size_t value);
void widget_set_custom_ptr(te_widget* widget, void* ptr);
size_t widget_get_custom_value(te_widget* widget);
void* widget_get_custom_ptr(te_widget* widget);

// Sets or changes the current parent of a widget. Specify NULL to remove parent.
// If the specified parent is spawned in some world but this widget is not spawned the widget will
// be spawned (added to world) and attached to the specified parent.
//
// If a widget is being destroyed it will also destroy all child widgets.
void widget_set_parent(te_widget* widget, te_widget* new_parent);
te_widget* widget_get_parent(te_widget* widget);

// Returns a pointer to a new array of child widgets.
// You must free returned array (but not the items in the array).
te_widget** widget_get_child_widgets(te_widget* widget, unsigned int* count);
unsigned int widget_get_child_widget_count(te_widget* widget);

// Optionally you can set a name of the widget. The string will be copied.
// Returns NULL if was not set previously.
void widget_set_name(te_widget* widget, const char* name);
const char* widget_get_name(te_widget* widget);

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

// Return `false` if this widget (and its child widgets) should not be serialized. `true` by default.
void widget_set_is_serialization_allowed(te_widget* widget, bool allow);
bool widget_is_serialization_allowed(te_widget* widget);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// Called to notify the widget about being spawned/despawned.
// Recursively call this function on all child nodes.
void prv_widget_on_spawned(te_widget* widget, struct te_world* world);
void prv_widget_on_despawned(te_widget* widget);

// Recursively calls this function on all child widgets.
void prv_widget_on_window_size_changed(te_widget* widget);

// Interactable widgets use this during their construction.
// Cursor pos here is in range [0.0; 1.0] relative to the window.
// Some callbacks may be specified as NULL.
void prv_widget_set_input_callbacks(
    te_widget* widget, void (*on_cursor_entered)(void* owner, vec2 cursor_pos),
    void (*on_cursor_left)(void* owner, vec2 cursor_pos),
    void (*on_mouse_button_pressed)(void* owner, enum te_mouse_button button, vec2 cursor_pos),
    void (*on_mouse_button_released)(
        void* owner, enum te_mouse_button button, vec2 cursor_pos),
    void (*on_hovered_cursor_moved)(void* owner, vec2 cursor_pos),
    void (*on_keyboard_input_text)(void* owner, const char* input_text),
    void (*on_keyboard_input)(void* owner, enum te_keyboard_button button));

// Called by world when the mouse cursor is inside of the widget. Cursor pos is position in range [0.0; 1.0] relative to the window.
void prv_widget_on_mouse_button_pressed(
    te_widget* widget, enum te_mouse_button button, vec2 cursor_pos);
void prv_widget_on_mouse_button_released(
    te_widget* widget, enum te_mouse_button button, vec2 cursor_pos);
void prv_widget_on_cursor_entered(te_widget* widget, vec2 cursor_pos);
void prv_widget_on_cursor_left(te_widget* widget, vec2 cursor_pos);
void prv_widget_on_hovered_cursor_moved(te_widget* widget, vec2 cursor_pos);
void prv_widget_on_keyboard_input(te_widget* widget, enum te_keyboard_button button);
void prv_widget_on_keyboard_input_text(te_widget* widget, const char* text);
