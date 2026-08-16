#pragma once

// Array with automatic size management specifically made for storing render data.
// Render data is stored linearly (to be cache friendly) and returned render data handles
// (that public API users use to modify render properties) never change even if new render items are added/removed.
typedef struct te_render_data_array te_render_data_array;

// Specify sizeof a single render data item and initial capacity of the array.
te_render_data_array* render_data_array_create(
    unsigned int sizeof_render_data, unsigned int init_capacity, unsigned int expand_count);
void render_data_array_destroy(te_render_data_array* array);

// Returns a handle which will never change and will always be valid until the item is removed.
// Use the handle to modify render data using @ref render_data_array_get_item_data_tmp.
unsigned int render_data_array_add_item(te_render_data_array* array);
void render_data_array_remove_item(te_render_data_array* array, unsigned int handle);

// Cast returned pointer to the appropriate type.
// Never store/save pointer to render data because on the next frame
// the pointer may end up pointing to an invalid memory. Only use "get_render_data" function to quickly
// update some render data. Suffix "_tmp" is used because of this.
// Do not free returned pointer, valid until the item is not removed using @ref render_data_array_remove_item.
void* render_data_array_get_item_data_tmp(te_render_data_array* array, unsigned int handle);

// Returns a pointer to the internal array that stores render data of all currently registered items.
// For the number of currently registered items use @ref render_data_array_get_item_count.
// Do not free returned pointer, valid while the array exists.
void* render_data_array_get_internal_array(te_render_data_array* array);

unsigned int render_data_array_get_item_count(te_render_data_array* array);
