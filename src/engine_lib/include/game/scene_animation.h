#pragma once

#include <type_database.h>
#include <game/game_object_info.h>

struct te_world;

// Scene animation animates world objects. It's generally created not using the code but using the editor,
// there's a special scene animation editor which allows creating, editing and playing such animations.
// Create or load scene animation using the world functions.
typedef struct te_scene_animation te_scene_animation;

enum te_scene_animation_interpolation_type {
    TE_SAIT_STEP = 0,
    TE_SAIT_LINEAR,
    TE_SAIT_CUBIC_SPLINE,

    TE_SAIT_COUNT, // <- marks the size of this enum
};

// Supported keyframe types:
#define SCENE_ANIM_KEYFRAME_TYPE(type, name)                                                  \
    typedef struct te_scene_animation_keyframe_##name {                                       \
        enum te_scene_animation_interpolation_type interpolation;                             \
        float time;                                                                           \
        type value;                                                                           \
    } te_scene_animation_keyframe_##name;
SCENE_ANIM_KEYFRAME_TYPE(bool, bool)
SCENE_ANIM_KEYFRAME_TYPE(unsigned, uint)
SCENE_ANIM_KEYFRAME_TYPE(float, float)
SCENE_ANIM_KEYFRAME_TYPE(vec2, vec2)
SCENE_ANIM_KEYFRAME_TYPE(vec3, vec3)
SCENE_ANIM_KEYFRAME_TYPE(vec4, vec4)

void scene_animation_set_is_looping(te_scene_animation* scene_animation, bool loop);
void scene_animation_play(te_scene_animation* scene_animation);
void scene_animation_pause(te_scene_animation* scene_animation);
void scene_animation_stop(te_scene_animation* scene_animation);

bool scene_animation_is_playing(te_scene_animation* scene_animation);
float scene_animation_get_current_time(te_scene_animation* scene_animation);

// Returns names of all objects animated in this scene animation.
// You must free the array pointer but not the individual strings.
char**
scene_animation_get_object_names(te_scene_animation* scene_animation, unsigned int* out_count);

// Returns names of all variables (of the object) animated in this scene animation.
// You must free the array pointer but not the individual strings.
char** scene_animation_get_bool_variable_names(
    te_scene_animation* scene_animation, const char* object_name, unsigned int* out_count);
char** scene_animation_get_uint_variable_names(
    te_scene_animation* scene_animation, const char* object_name, unsigned int* out_count);
char** scene_animation_get_float_variable_names(
    te_scene_animation* scene_animation, const char* object_name, unsigned int* out_count);
char** scene_animation_get_vec2_variable_names(
    te_scene_animation* scene_animation, const char* object_name, unsigned int* out_count);
char** scene_animation_get_vec3_variable_names(
    te_scene_animation* scene_animation, const char* object_name, unsigned int* out_count);
char** scene_animation_get_vec4_variable_names(
    te_scene_animation* scene_animation, const char* object_name, unsigned int* out_count);

#define SCENE_ANIM_ADD_KEYFRAME_FUNC(type, name)                                              \
    void scene_animation_add_keyframe_##name(                                                 \
        te_scene_animation* scene_animation, const char* object_name,                         \
        const char* variable_name, float time, type value);
SCENE_ANIM_ADD_KEYFRAME_FUNC(bool, bool)
SCENE_ANIM_ADD_KEYFRAME_FUNC(unsigned int, uint)
SCENE_ANIM_ADD_KEYFRAME_FUNC(float, float)
SCENE_ANIM_ADD_KEYFRAME_FUNC(vec2, vec2)
SCENE_ANIM_ADD_KEYFRAME_FUNC(vec3, vec3)
SCENE_ANIM_ADD_KEYFRAME_FUNC(vec4, vec4)

// Do not delete returned pointer, valid until keyframes are not modified.
#define SCENE_ANIM_GET_KEYFRAMES(var_type)                                                    \
    te_scene_animation_keyframe_##var_type* scene_animation_get_keyframes_##var_type(         \
        te_scene_animation* scene_animation, const char* object_name,                         \
        const char* variable_name, unsigned int* out_keyframe_count);
SCENE_ANIM_GET_KEYFRAMES(bool)
SCENE_ANIM_GET_KEYFRAMES(uint)
SCENE_ANIM_GET_KEYFRAMES(float)
SCENE_ANIM_GET_KEYFRAMES(vec2)
SCENE_ANIM_GET_KEYFRAMES(vec3)
SCENE_ANIM_GET_KEYFRAMES(vec4)

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// World is supposed to create/load scene animations.
te_scene_animation* prv_scene_animation_create(struct te_world* world);
void prv_scene_animation_destroy(te_scene_animation* scene_animation);
