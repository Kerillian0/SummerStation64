/**
 * @file achievements.h
 * @brief A game's achievements, as `ra_bridge.exe --prepare` put them on the
 * SD card (pc/ra_bridge/prep.h): which RetroAchievements game a ROM is
 * (menu/achievements/index.txt, read once per power-on), that game's list
 * (<id>/set.txt, loaded while its page is open), and which of them the
 * player has unlocked (the profile's achievements.txt: "achunlocked 1",
 * then "game <TAB> id <TAB> day" lines, days since 2000; written by the
 * local checker, still to come).
 */

#ifndef ACHIEVEMENTS_H__
#define ACHIEVEMENTS_H__

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ACH_NONE,               /**< not on the card's list, or RetroAchievements doesn't know it */
    ACH_OTHER_DUMP,         /**< RetroAchievements has the game, but not for this copy */
    ACH_SET,                /**< a set is on the card (it may hold no achievements yet) */
} ach_kind_t;

typedef enum {
    ACH_TYPE_PLAIN, ACH_TYPE_MISSABLE, ACH_TYPE_PROGRESSION, ACH_TYPE_WIN,
} ach_type_t;

typedef struct {
    uint32_t id;
    uint16_t points;
    uint8_t type;               /**< ach_type_t */
    const char *badge;          /**< the badge's file name, without .png */
    const char *title;
    const char *description;
} achievement_t;

/** What the card holds for the ROM at this full path ("sd:/N64(US)/Game.z64"). */
ach_kind_t achievements_for_rom (const char *path, uint32_t *game_id);

/** Load a game's list (frees the one before). @return false if it can't be read. */
bool achievements_set_load (uint32_t game_id);
void achievements_set_free (void);

/** The loaded game's name, as RetroAchievements writes it. */
const char *achievements_set_title (void);
int achievements_count (void);
const achievement_t *achievements_get (int i);

/** Whether the player has unlocked achievement `i` of the loaded game; `day` when (days since 2000, 0 unknown). */
bool achievements_unlocked (int i, uint16_t *day);

/** Unlocked so far in the loaded game, and their points; and all its points. */
void achievements_progress (int *unlocked, int *points, int *total_points);

#endif
