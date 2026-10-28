/**
 * @file card_music.h
 * @brief A theme's music played from the SD card without crackling.
 *
 * libdragon streams a wav64 from the card a few hundred bytes at a time,
 * inside the sound mixing, so a busy moment on the card (loading a game's
 * details, a cover) starves the music and it crackles. Here the music is
 * read ahead into a ring in memory in large pieces, one piece a frame
 * (card_music_fill()), and the mixer takes its samples from the ring.
 * For uncompressed 16-bit wav64 files (what the Companion's Sounds tab
 * makes); anything else is left to wav64 streaming as before.
 */

#ifndef CARD_MUSIC_H__
#define CARD_MUSIC_H__

#include <stdbool.h>
#include <libdragon.h>

/** Open `path` for looped playback; false when it is not a file this can play (or memory is short). */
bool card_music_open (const char *path);

/** The waveform to hand to mixer_ch_play() after card_music_open(). */
waveform_t *card_music_wave (void);

/** Read the next piece into the ring when there is room (call once a frame). */
void card_music_fill (void);

/** Stop the music and free the ring. */
void card_music_close (void);

#endif /* CARD_MUSIC_H__ */
