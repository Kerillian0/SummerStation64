/**
 * @file cheat_ready.h
 * @brief The cheats a game starts with, when Use Cheats is on.
 *
 * The stock menu only reads a game's cheat file ("Game.datel", or
 * "Game.datel.txt", beside the game) when its Datel Code Editor is opened,
 * so Use Cheats alone started the game with no codes, or with the last
 * game's. This reads the file as the game starts, unless the editor
 * already loaded this game's codes (perhaps changed, not saved).
 */

#ifndef CHEAT_READY_H__
#define CHEAT_READY_H__

/** The Datel Code Editor has loaded the cheats of the game at this full path. */
void cheat_ready_note (const char *rom_path);

/** Just before the game at this path starts: its cheats from the file, if not loaded already. */
void cheat_ready_load (const char *rom_path);

#endif /* CHEAT_READY_H__ */
