#pragma once

#include <cglm/mat4.h>
#include <cglm/vec3.h>

struct te_game_manager;

// Skeleton for skeletal animations.
typedef struct te_skeleton te_skeleton;
typedef struct te_skeleton_animation te_skeleton_animation;

// Preloads animation(s) from the specified file/directory (relative to the `res` directory)
// to be later played using @ref skeleton_play_animation.
void skeleton_load_animations(te_skeleton* skeleton, const char* relative_path);

// Plays an animation (that was previously loaded using @ref skeleton_load_animations)
// accepts animation file name (without extension).
void skeleton_play_animation(te_skeleton* skeleton, const char* anim_name, bool loop);

// Stops any currently playing animation.
void skeleton_stop_animation(te_skeleton* skeleton);

// Stops playing animation and unloads all previously loaded animations (from @ref skeleton_load_animations).
void skeleton_unload_animations(te_skeleton* skeleton);

// Returns the total number of bones the skeleton has.
unsigned int skeleton_get_bone_count(te_skeleton* skeleton);

// Returns an array where each matrix stores the final transform of a bone (to be passed to the shader).
// Matrices in this array will be updated every time @ref skeleton_update is called.
// Do not free returned pointer, valid while the skeleton is not destroyed.
mat4* skeleton_get_skinning_mats(te_skeleton* skeleton);

// ------------------------------------------------------------------------------------------------
//                                       PRIVATE API
// ------------------------------------------------------------------------------------------------

// same as in skeleton.vert.glsl
// you can increase this value if needed (but also update the shader)
#define TE_MAX_BONE_COUNT 80

enum te_animation_channel_type {
    TE_ACT_POSITION = 0,
    TE_ACT_ROTATION, // in degrees
    TE_ACT_SCALE,

    TE_ACT_COUNT, // <- marks the size of this enum
};

enum te_keyframe_interpolation_type {
    TE_KIT_STEP = 0,
    TE_KIT_LINEAR,
    TE_KIT_CUBIC_SPLINE,

    TE_KIT_COUNT, // <- marks the size of this enum
};

// Loads skeleton from a file relative to the `res` directory.
te_skeleton* prv_skeleton_create(const char* relative_path, struct te_game_manager* game_manager);
void prv_skeleton_destroy(te_skeleton* skeleton);

// Updates inverse bind pose matrices for all skeleton bones and calculates skinning matrices
// based on the current state of a skeleton (if playing an animation fills matrices
// with animated bone transforms at the current animation time).
//
// Provided @ref skinning_mats must have N matrices where N is the number of bones the skeleton has.
void prv_skeleton_update(te_skeleton* skeleton, float dt);