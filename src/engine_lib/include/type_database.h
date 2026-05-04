#pragma once

#include <stdbool.h>
#include <cglm/vec2.h>
#include <cglm/vec3.h>
#include <cglm/vec4.h>
#include <wchar.h>

struct te_config;
struct te_widget;
struct te_world;

enum te_variable_type {
    TE_VT_BOOL,
    TE_VT_UINT,
    TE_VT_FLOAT,
    TE_VT_VEC2,
    TE_VT_VEC3,
    TE_VT_VEC4,
    TE_VT_STRING,
    TE_VT_WSTRING,
};

typedef void (*te_bool_setter)(void* obj, bool val);
typedef bool (*te_bool_getter)(void* obj);

typedef void (*te_uint_setter)(void* obj, unsigned int val);
typedef unsigned int (*te_uint_getter)(void* obj);

typedef void (*te_float_setter)(void* obj, float val);
typedef float (*te_float_getter)(void* obj);

typedef void (*te_vec2_setter)(void* obj, vec2 val);
typedef void (*te_vec2_getter)(void* obj, vec2 out);

typedef void (*te_vec3_setter)(void* obj, vec3 val);
typedef void (*te_vec3_getter)(void* obj, vec3 out);

typedef void (*te_vec4_setter)(void* obj, vec4 val);
typedef void (*te_vec4_getter)(void* obj, vec4 out);

typedef void (*te_string_setter)(void* obj, const char* val);
typedef const char* (*te_string_getter)(void* obj);

typedef void (*te_wstring_setter)(void* obj, const wchar_t* val);
typedef const wchar_t* (*te_wstring_getter)(void* obj);

typedef struct te_variable_info {
    const char* name;

    enum te_variable_type type;

    // Index into the setters/getters array of function pointers from @ref te_type_info.
    unsigned short set_get_index;
} te_variable_info;

typedef struct te_type_info {
    // Unique identifier of the type.
    const char* id;

    void* (*create)(void);
    void (*destroy)(void* obj);

    void (*spawn)(struct te_world* world, void* obj);
    void (*despawn)(struct te_world* world, void* obj);

    // NULL if not a widget, otherwise returns base widget type.
    struct te_widget* (*get_widget)(void* obj);

    // Returns `false` if this object should not be serialized.
    bool (*is_serialization_allowed)(void* obj);

    te_variable_info* variables;

    te_bool_setter* bool_setters;
    te_bool_getter* bool_getters;

    te_uint_setter* uint_setters;
    te_uint_getter* uint_getters;

    te_float_setter* float_setters;
    te_float_getter* float_getters;

    te_vec2_setter* vec2_setters;
    te_vec2_getter* vec2_getters;

    te_vec3_setter* vec3_setters;
    te_vec3_getter* vec3_getters;

    te_vec4_setter* vec4_setters;
    te_vec4_getter* vec4_getters;

    te_string_setter* string_setters;
    te_string_getter* string_getters;

    te_wstring_setter* wstring_setters;
    te_wstring_getter* wstring_getters;

    unsigned short variable_count;

    unsigned short bool_count;
    unsigned short uint_count;
    unsigned short float_count;
    unsigned short vec2_count;
    unsigned short vec3_count;
    unsigned short vec4_count;
    unsigned short string_count;
    unsigned short wstring_count;
} te_type_info;

// Creates a new type info to be registered using @ref type_database_register_type.
// Specify NULL to get_widget if not a widget, otherwise return base widget type.
te_type_info* type_info_create(
    const char* id,
    void* (*create)(void),
    void (*destroy)(void* obj),
    void (*spawn)(struct te_world* world, void* obj),
    void (*despawn)(struct te_world* world, void* obj),
    struct te_widget* (*get_widget)(void*),
    bool (*is_serialization_allowed)(void* obj));
void type_info_add_bool_variable(
    te_type_info* info, const char* name, te_bool_setter setter, te_bool_getter getter);
void type_info_add_uint_variable(
    te_type_info* info, const char* name, te_uint_setter setter, te_uint_getter getter);
void type_info_add_float_variable(
    te_type_info* info, const char* name, te_float_setter setter, te_float_getter getter);
void type_info_add_vec2_variable(
    te_type_info* info, const char* name, te_vec2_setter setter, te_vec2_getter getter);
void type_info_add_vec3_variable(
    te_type_info* info, const char* name, te_vec3_setter setter, te_vec3_getter getter);
void type_info_add_vec4_variable(
    te_type_info* info, const char* name, te_vec4_setter setter, te_vec4_getter getter);
void type_info_add_string_variable(
    te_type_info* info, const char* name, te_string_setter setter, te_string_getter getter);
void type_info_add_wstring_variable(
    te_type_info* info, const char* name, te_wstring_setter setter, te_wstring_getter getter);

// Creates a new section in the specified config (returns index of the created section) and saves all reflected variables
// in this new section.
unsigned int
type_info_save_to_config(const te_type_info* type_info, struct te_config* config, void* obj);

// Loads variables from the specified config section into the specified object.
void type_info_load_from_config(
    const te_type_info* type_info, struct te_config* config, unsigned int section_idx,
    void* obj);

// Registers the specified type. Ownership of the pointer is moved to the type database.
void type_database_register_type(te_type_info* info);

// Returns NULL if not registered. Do not free/destroy returned pointer.
const te_type_info* type_database_get_type_info(const char* id);

// Returns array (which you need to free) to static strings (which you don't need to free)
// to IDs of all currently registered types.
const char** type_database_get_all_type_ids(unsigned int* count);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

void prv_type_database_init(void);
void prv_type_database_deinit(void);
