/**
 * @file dump_check.h
 * @brief Whether a game file is a known good dump.
 *
 * The Card Prep web page (docs/companion/, the Companion's Card Prep tab) checks every game on the card against
 * No-Intro's list and writes sd:/menu/verified.txt; this reads it once and
 * answers per file. A file that differs from the list is "not verified"
 * unless its name says why (a fix, translation, trainer or hack: no mark)
 * or that it is bad (a GoodN64 [b] tag: "bad dump"). With no verified.txt,
 * nothing is marked. Feature `dump_marks`.
 */

#ifndef DUMP_CHECK_H__
#define DUMP_CHECK_H__

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DUMP_NONE,          /**< nothing to show: not checked, homebrew, or changed on purpose */
    DUMP_VERIFIED,      /**< exactly a known good dump: green check */
    DUMP_UNVERIFIED,    /**< differs from the known dump, cause unknown: amber question mark */
    DUMP_BAD,           /**< named as a bad dump: red cross */
} dump_state_t;

/**
 * What to show for this file (its full path, "sd:/..."). `size` is the
 * file's size, or 0 or less if not known. `hack`: the menu took it
 * for a hack, which explains a difference.
 */
dump_state_t dump_check (const char *path, int64_t size, bool hack);

/** A short word for the state ("Verified", ...), or NULL for DUMP_NONE. */
const char *dump_check_label (dump_state_t state);

#endif /* DUMP_CHECK_H__ */
