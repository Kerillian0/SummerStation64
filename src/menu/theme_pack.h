/**
 * @file theme_pack.h
 * @brief Sounds and a font that come with a theme on the SD card.
 *
 * A card theme's folder (sd:/menu/themes/<name>/, see card_themes.h) may
 * hold any of these beside its theme.ini; whatever it leaves out is the
 * menu's own:
 *   music.wav64    the background music (loops)
 *   intro.wav64    the power-on tune
 *   move.wav64     moving the cursor
 *   select.wav64   choosing something
 *   back.wav64     going back
 *   setting.wav64  changing a setting
 *   error.wav64    something went wrong
 *   font.font64    the menu's text font (applies after a restart)
 * Button sounds and the intro tune are read into memory whole (at most 48 KB
 * each and 160 KB on a 4MB console, 128 KB and 768 KB with the Expansion
 * Pak); music is read ahead a piece at a time (card_music.h).
 * Only music.wav64 should loop; the others are meant to play once (the intro
 * is told not to loop, but a file made with a loop point may still repeat).
 * Sounds are wav64 files, made from any audio file by the Companion's
 * Sound Pack tool (or libdragon's audioconv64). A file is checked before
 * it is used; one the menu can't play is skipped, so a bad file never
 * stops the menu.
 */

#ifndef THEME_PACK_H__
#define THEME_PACK_H__

#include <libdragon.h>

/** The file to play for one of the sounds above: the theme's own if it has a good one, else `rom_path`. */
const char *theme_pack_sound (const char *file_name, const char *rom_path);

/** A button sound: the theme's own read into memory whole (if it fits), else `rom_path` streamed as before. */
wav64_t *theme_pack_effect (const char *file_name, const char *rom_path);

/** The intro tune to play (once): the theme's own read into memory whole if it fits, else the menu's. Call theme_pack_intro_free() after stopping its channel. */
waveform_t *theme_pack_intro (void);

/** Free the intro tune. */
void theme_pack_intro_free (void);

/** Note how long one round of sound mixing took; the log gets the worst each second when it is 8 ms or more. */
void theme_pack_note_mixing (int ms);

/** The theme's font file, or NULL when it has none (or it is too large for this console). */
const char *theme_pack_font (void);

#endif /* THEME_PACK_H__ */
