/**
 * @file review_scores.h
 * @brief A table of review scores per game code, read from a file the
 * player provides (docs/review_scores.md). Plain C with no libdragon, so it
 * is tested on the PC (tests/review_scores_test.c).
 *
 * The file:
 *
 *     reviewscores 1
 *     # comments start with #
 *     NSME 87
 *     NZLE 9.9
 *
 * The first line is the header. Then one game per line: the four-letter
 * game code from the cartridge header, then its score out of 10, either in
 * tenths (87) or with a point (8.7). A code listed twice keeps its first
 * score; lines that don't read as a code and a score from 0.1 to 10 are
 * skipped.
 */

#ifndef REVIEW_SCORES_H__
#define REVIEW_SCORES_H__

#include <stdbool.h>
#include <stdio.h>

/** Most codes kept: every N64 cartridge code with room to spare. */
#define REVIEW_SCORES_MAX   (4096)

/**
 * Read a scores file, replacing any table read before.
 * @return false (and an empty table) if it does not start with the header.
 */
bool review_scores_read (FILE *f);

/** The score in tenths (87 = 8.7 out of 10) for a four-letter game code, 0 if it has none. */
int review_scores_find (const char *code);

/** How many codes have a score. */
int review_scores_count (void);

/** Forget the table. */
void review_scores_clear (void);

#endif /* REVIEW_SCORES_H__ */
