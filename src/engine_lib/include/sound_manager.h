#pragma once

#include <stdbool.h>
#include <math/vec3.h>

/* ------------------------------------------------------------------------------------------------
 *                                   SOUND MANAGER API
 * ------------------------------------------------------------------------------------------------ */

typedef struct te_sound_manager te_sound_manager;

te_sound_manager* sound_manager_create(void);
void sound_manager_destroy(te_sound_manager* sound_manager);

/* sets the master volume (of all sounds) where 0 means silent,
 * 1 - unchanged audio volume and 1+ will result in volume amplification */
void sound_manager_set_volume(te_sound_manager* sound_manager, float volume);

/* ------------------------------------------------------------------------------------------------
 *                                       SOUND API
 * ------------------------------------------------------------------------------------------------ */

typedef struct te_sound te_sound;

/* loads audio from file (path relative to the `res` directory) */
te_sound* sound_create(te_sound_manager* sound_manager, const char* relative_path);
void sound_destroy(te_sound* sound);

void sound_play(te_sound* sound);
void sound_stop(te_sound* sound);
bool sound_is_playing(te_sound* sound);

/* sets sound volume where 0 means silent,
 * 1 - unchanged audio volume and 1+ will result in volume amplification */
void sound_set_volume(te_sound* sound, float volume);

void sound_set_is_looping(te_sound* sound, bool is_looping);

/* sets sound stereo panning. 0 means unpanned sound
 * -1 will shift everything to the left, whereas +1 will shift it to the right */
void sound_set_pan(te_sound* sound, float pan);

/* a larger value will result in a higher pitch, the pitch must be greater than 0 */
void sound_set_pitch(te_sound* sound, float pitch);

/* returns `true` if the sound was started and is now finished playing
 * this will never return `true` for looping sound */
bool sound_is_finished_playing(te_sound* sound);

/* sets position of the sound in 3D world (enables spatialization)
 * generally a world object will handle this for you */
void sound_set_3d_position(te_sound* sound, te_vec3 pos);

/* enables spatialization and sets the minimum and maximum distances
 * for the attenuation calculation (see @ref sound_set_3d_position) */
void sound_set_distance(te_sound* sound, float min, float max);

/* ------------------------------------------------------------------------------------------------
 *                                       PRIVATE API
 * ------------------------------------------------------------------------------------------------ */

void prv_sound_manager_set_listener(
    te_sound_manager* sound_manager, te_vec3 pos, te_vec3 forward, te_vec3 up);

/* sets callback that will be triggered once the sound is finished
 * this will never be called for looping sound
 * 
 * Warning: if some system has already set this callback calling this again will overwrite the callback.
 * Warning: the callback will NOT be called from the main thread but from the audio thread. You cannot
 * uninitialize/destroy sounds from that thread. */
void prv_sound_set_on_finished_callback_audio_thread(
    te_sound* sound, void* user_data, void (*on_finished)(void* user_data, te_sound* sound));
