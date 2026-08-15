#include <render/render_data_array.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <io/log.h>

#define INVALID_DATA_INDEX 0xFFFFFFFF

struct te_render_data_array {
    // Array of render data of all currently registered items.
    //
    // Max size of this array is @ref array_size but the actual number of valid
    // (used) elements might be different (see @ref render_data_count). When some render data is removed all next
    // elements are shifted to the left to make sure the array does not have any "holes".
    // This array does not shrink.
    void* render_data;

    // Index into this array using a handle to get index into @ref render_data.
    //
    // Public API users store indices into this array so items cannot be reordered/moved.
    // This array CAN have "holes" in it (invalid items). Invalid items store INVALID_DATA_INDEX value.
    // This array does not shrink. Size of this array is @ref array_size.
    unsigned int* handle_to_data;

    // Max number of elements in the arrays @ref render_data and @ref handle_to_data.
    unsigned int array_size;

    // sizeof for a single render data item.
    unsigned int sizeof_render_data;

    // Actual number of used (valid) elements in @ref render_data.
    unsigned int render_data_count;

    unsigned int expand_count;
};

te_render_data_array*
render_data_array_create(
    unsigned int sizeof_render_data, unsigned int init_capacity, unsigned int expand_count) {
    te_render_data_array* array = malloc(sizeof(te_render_data_array));

    array->sizeof_render_data = sizeof_render_data;
    array->expand_count = expand_count;
    array->array_size = init_capacity;
    array->render_data = malloc(sizeof_render_data * array->array_size);
    array->handle_to_data = malloc(sizeof(unsigned int) * array->array_size);
    for (unsigned int i = 0; i < array->array_size; i++) {
        array->handle_to_data[i] = INVALID_DATA_INDEX;
    }
    array->render_data_count = 0;

    return array;
}

void
render_data_array_destroy(te_render_data_array* array) {
    free(array->render_data);
    free(array->handle_to_data);
    free(array);
}

unsigned int
render_data_array_add_item(te_render_data_array* array) {
    // Find unused handle.
    unsigned int handle = 0;
    bool found = false;
    for (unsigned int i = 0; i < array->array_size; i++) {
        if (array->handle_to_data[i] != INVALID_DATA_INDEX) {
            continue;
        }
        handle = i;
        found = true;
        break;
    }
    if (!found) {
        unsigned int* new_handles =
            malloc(sizeof(unsigned int) * (array->array_size + array->expand_count));
        memcpy(new_handles, array->handle_to_data, sizeof(unsigned int) * array->array_size);

        free(array->handle_to_data);
        array->handle_to_data = new_handles;

        void* new_data =
            malloc(array->sizeof_render_data * (array->array_size + array->expand_count));
        memcpy(
            new_data, array->render_data,
            array->sizeof_render_data * array->render_data_count);

        free(array->render_data);
        array->render_data = new_data;

        for (unsigned int i = array->array_size; i < array->array_size + array->expand_count;
             i++) {
            array->handle_to_data[i] = INVALID_DATA_INDEX;
        }

        handle = array->array_size;
        array->array_size += array->expand_count;
    }

    array->handle_to_data[handle] = array->render_data_count;
    array->render_data_count += 1;

    return handle;
}

void
render_data_array_remove_item(te_render_data_array* array, unsigned int handle) {
#if defined(DEBUG)
    if (handle >= array->array_size) {
        log_error("invalid render data handle");
        abort();
    }
#endif

    const unsigned int render_data_index = array->handle_to_data[handle];

#if defined(DEBUG)
    if (render_data_index == INVALID_DATA_INDEX) {
        log_error("invalid render data handle");
        abort();
    }
#endif

    array->handle_to_data[handle] = INVALID_DATA_INDEX;

    if (array->render_data_count > 1) {
        // Remove "hole" from the array.
        char* render_data = array->render_data; // <- cast from void*
        memmove(
            render_data + array->sizeof_render_data * render_data_index,
            render_data + array->sizeof_render_data * (render_data_index + 1),
            array->sizeof_render_data * (array->render_data_count - render_data_index - 1));
    }

    // Shift render data indices after the removed one.
    for (unsigned int i = 0; i < array->array_size; i++) {
        if (array->handle_to_data[i] == INVALID_DATA_INDEX
            || array->handle_to_data[i] < render_data_index) {
            continue;
        }
        array->handle_to_data[i] -= 1;
    }

    array->render_data_count -= 1;
}

void*
render_data_array_get_item_data_tmp(te_render_data_array* array, unsigned int handle) {
#if defined(DEBUG)
    if (handle >= array->array_size) {
        log_error("invalid render data handle");
        abort();
    }
#endif

    char* render_data = array->render_data; // <- cast from void*
    return render_data + array->sizeof_render_data * array->handle_to_data[handle];
}

unsigned int
render_data_array_get_item_count(te_render_data_array* array) {
    return array->render_data_count;
}

void*
render_data_array_get_internal_array(te_render_data_array* array) {
    return array->render_data;
}
