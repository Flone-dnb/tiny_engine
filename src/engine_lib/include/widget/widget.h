#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <math/vec2.h>
#include <input/keyboard_button.h>
#include <input/mouse_button.h>

/* core component of any widget, this type handles hierarchy functionality
 * widgets store (own) this object inside of them
 * 
 * generally you should not create this object directly, other widgets like text or button
 * will create this object themselves */
typedef struct te_widget te_widget;
struct te_world;

/* callbacks that will be triggered when the widget is modified, some may be specified as NULL
 * all callbacks specified here will only be called while the widget is spawned except for the "on before base destroyed" callback
 * "pos/size changed" callbacks will be triggered in both cases: when relative or screen pos/size changes
 * "on after spawned" callback is called before any child widget is spawned
 * "on before despawned" callback is called after all child widgets are despawned */
te_widget* widget_create(
    void* owner, const char* (*get_type_id)(void), void (*on_pos_changed)(void* owner),
    void (*on_size_changed)(void* owner), void (*on_before_base_destroyed)(void* owner),
    void (*on_parent_changed)(void* owner), void (*on_children_changed)(void* owner),
    void (*on_after_spawned)(void* owner), void (*on_before_despawned)(void* owner),
    void (*on_window_size_changed)(void* owner));
void widget_destroy(te_widget* widget);

/* returns the actual widget object that owns this base widget */
void* widget_get_owner(te_widget* widget);
const char* widget_get_owner_type_id(te_widget* widget);

/* used to add a custom (user-defined) value to the widget */
void widget_set_custom_value(te_widget* widget, size_t value);
void widget_set_custom_ptr(te_widget* widget, void* ptr);
size_t widget_get_custom_value(te_widget* widget);
void* widget_get_custom_ptr(te_widget* widget);

/* sets or changes the current parent of a widget, specify NULL to remove parent
 * if the specified parent is spawned in some world but this widget is not spawned the widget will
 * be spawned (added to world) and attached to the specified parent
 * 
 * if a widget is being destroyed it will also destroy all child widgets */
void widget_set_parent(te_widget* widget, te_widget* new_parent);
te_widget* widget_get_parent(te_widget* widget);

/* returns a pointer to a new array of child widgets
 * you must free returned array (but not the items in the array) */
te_widget** widget_get_child_widgets(te_widget* widget, unsigned int* count);
unsigned int widget_get_child_widget_count(te_widget* widget);

/* optionally you can set a name of the widget, the string will be copied
 * returns NULL if was not set previously */
void widget_set_name(te_widget* widget, const char* name);
const char* widget_get_name(te_widget* widget);

/* sets position of the widget in range [0.0; 1.0] relative to the window's top-left corner
 * if the widget has a parent then this position becomes relative to the parent's position/size */
void widget_set_relative_position(te_widget* widget, te_vec2 position);
void widget_get_relative_position(te_widget* widget, te_vec2 out);

/* sets size of the widget in range [0.0; 1.0] relative to the window's top-left corner
 * if the widget has a parent then this size becomes relative to the parent's size */
void widget_set_relative_size(te_widget* widget, te_vec2 size);
void widget_get_relative_size(te_widget* widget, te_vec2 out);

/* returns widget's position and size in range [0.0; 1.0] relative to the window's size
 * includes transformations of all parents (if the widget has parents) */
void widget_get_screen_position(te_widget* widget, te_vec2 pos);
void widget_get_screen_size(te_widget* widget, te_vec2 size);

/* returns NULL if not spawned */
struct te_world* widget_get_world(te_widget* widget);

/* return `false` if this widget (and its child widgets) should not be serialized. `true` by default */
void widget_set_is_serialization_allowed(te_widget* widget, bool allow);
bool widget_is_serialization_allowed(te_widget* widget);

/* ------------------------------------------------------------------------------------------------
 *                                       PRIVATE API
 * ------------------------------------------------------------------------------------------------ */

/* called to notify the widget about being spawned/despawned
 * recursively call this function on all child nodes */
void prv_widget_on_spawned(te_widget* widget, struct te_world* world);
void prv_widget_on_despawned(te_widget* widget);

/* recursively calls this function on all child widgets */
void prv_widget_on_window_size_changed(te_widget* widget);

/* interactable widgets use this during their construction
 * cursor pos here is in range [0.0; 1.0] relative to the window
 * some callbacks may be specified as NULL */
void prv_widget_set_input_callbacks(
    te_widget* widget, void (*on_cursor_entered)(void* owner, te_vec2 cursor_pos),
    void (*on_cursor_left)(void* owner, te_vec2 cursor_pos),
    void (*on_mouse_button_pressed)(
        void* owner, enum te_mouse_button button, te_vec2 cursor_pos),
    void (*on_mouse_button_released)(
        void* owner, enum te_mouse_button button, te_vec2 cursor_pos),
    void (*on_hovered_cursor_moved)(void* owner, te_vec2 cursor_pos),
    void (*on_keyboard_input_text)(void* owner, const char* input_text),
    void (*on_keyboard_input)(void* owner, enum te_keyboard_button button));

/* called by world when the mouse cursor is inside of the widget
 * cursor pos is position in range [0.0; 1.0] relative to the window */
void prv_widget_on_mouse_button_pressed(
    te_widget* widget, enum te_mouse_button button, te_vec2 cursor_pos);
void prv_widget_on_mouse_button_released(
    te_widget* widget, enum te_mouse_button button, te_vec2 cursor_pos);
void prv_widget_on_cursor_entered(te_widget* widget, te_vec2 cursor_pos);
void prv_widget_on_cursor_left(te_widget* widget, te_vec2 cursor_pos);
void prv_widget_on_hovered_cursor_moved(te_widget* widget, te_vec2 cursor_pos);
void prv_widget_on_keyboard_input(te_widget* widget, enum te_keyboard_button button);
void prv_widget_on_keyboard_input_text(te_widget* widget, const char* text);
