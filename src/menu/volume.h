/**
 * @file volume.h
 * @brief Separate loudness for the music and for the button sounds.
 *
 * Two sliders in Settings > Sound, 0 to 10 each (options `music_volume` and
 * `sound_volume`). 5, the default, is the level the menu has always played
 * at; 10 is twice that, 0 is silent. The intro tune follows the button
 * sounds' slider.
 */

#ifndef VOLUME_H__
#define VOLUME_H__

#define VOLUME_STEPS    (10)    /**< the sliders go from 0 to this */

/** How loud to play the music, as the mixer wants it (0 = silent). */
float volume_music (void);

/** How loud to play the button sounds and the intro tune. */
float volume_effects (void);

/** Put both levels into effect at once (after a slider moved). */
void volume_apply (void);

#endif /* VOLUME_H__ */
