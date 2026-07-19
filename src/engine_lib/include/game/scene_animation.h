#pragma once

#include <type_database.h>
#include <game/game_object_info.h>

// Scene animation animates world objects. It's generally created not using the code but using the editor,
// there's a special scene animation editor which allows creating, editing and playing such animations.
typedef struct te_scene_animation te_scene_animation;

enum te_scene_animation_interpolation_type {
    TE_SAIT_STEP = 0,
    TE_SAIT_LINEAR,
    TE_SAIT_CUBIC_SPLINE,

    TE_SAIT_COUNT, // <- marks the size of this enum
};

// Supported keyframe types:
#define SCENE_ANIM_KEYFRAME_TYPE(type)                                                        \
    typedef struct te_scene_animation_keyframe_##type {                                       \
        enum te_scene_animation_interpolation_type interpolation;                             \
        float time;                                                                           \
        type value;                                                                           \
    } te_scene_animation_keyframe_##type;
SCENE_ANIM_KEYFRAME_TYPE(bool)
SCENE_ANIM_KEYFRAME_TYPE(unsigned)
SCENE_ANIM_KEYFRAME_TYPE(float)
SCENE_ANIM_KEYFRAME_TYPE(vec2)
SCENE_ANIM_KEYFRAME_TYPE(vec3)
SCENE_ANIM_KEYFRAME_TYPE(vec4)

te_scene_animation* scene_animation_create();
void scene_animation_destroy(te_scene_animation* scene_animation);

void scene_animation_add_keyframe_bool(
    te_scene_animation* scene_animation, const char* object_name, const char* variable_name,
    float time, bool value);

// Do not delete returned pointer, valid until keyframes are not modified.
te_scene_animation_keyframe_bool* scene_animation_get_keyframes_bool(
    te_scene_animation* scene_animation, const char* object_name, const char* variable_name,
    unsigned int* out_keyframe_count);
