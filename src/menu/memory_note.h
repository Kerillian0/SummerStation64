/**
 * @file memory_note.h
 * @brief Remembers what the menu turned down because memory was short.
 *
 * When memory runs short the menu quietly does without a few extras (side
 * cover art, baked covers, the intro's title picture) instead of failing.
 * Each of those places reports it here, and Settings > System > Memory says
 * so, along with how much memory is free. Kept until the console is
 * switched off; nothing is saved.
 */

#ifndef MEMORY_NOTE_H__
#define MEMORY_NOTE_H__

typedef enum {
    MEMORY_CUT_SIDE_ART,        /**< side covers shown without their art */
    MEMORY_CUT_BAKED_ART,       /**< a baked cover was not loaded */
    MEMORY_CUT_INTRO_PICTURE,   /**< the intro's title shown as text */
    MEMORY_CUT_COUNT
} memory_cut_t;

/** Note that something was turned down. Logged the first time only. */
void memory_note_cut (memory_cut_t what);

/** For the Settings row: free memory now, e.g. "612 KB free". */
const char *memory_note_value (void);

/** For the Settings help line: what was turned down, or that nothing was. */
const char *memory_note_details (void);

#endif /* MEMORY_NOTE_H__ */
