/**
 * @file volume.h
 * @brief Separate loudness for the music and for the button sounds.
 *
 * Two sliders in Settings > Sound, 0 to 10 each (options `music_volume` and
 * `sound_volume`). 5, the default, is the level the menu has always played
 * at; 10 is twice that, 0 is silent. The intro tune follows the button
 * sounds' slider.
 *
 * Music Boost (option `music_boost`) raises the music on top of its slider,
 * in steps of 3 dB up to +12 dB (about four times as loud), for music that
 * was mastered quietly. The mixer can't play a channel louder than full, so
 * a boosted slider near the top stops there.
 */

#ifndef VOLUME_H__
#define VOLUME_H__

#define VOLUME_STEPS    (10)    /**< the sliders go from 0 to this */

/** How loud to play the music, as the mixer wants it (0 = silent). */
float volume_music (void);

/** How loud to play the button sounds and the intro tune. */
float volume_effects (void);

#define VOLUME_BOOST_STEPS  (5)     /**< Music Boost: Off, +3, +6, +9, +12 dB */

/** Music Boost's name for step `i` ("Off", "+3 dB", ...), for Settings. */
const char *volume_boost_name (int i);

/** Put both levels into effect at once (after a slider moved). */
void volume_apply (void);

#endif /* VOLUME_H__ */
