/**
 * @file safe_mode.h
 * @brief Safe mode: hold Z while the menu starts.
 *
 * Ignores theme.ini, the user's feature choices and the custom background
 * image, so a bad file on the SD card can't lock anyone out of the menu.
 * Nothing on the SD card is changed or deleted.
 */

#ifndef SAFE_MODE_H__
#define SAFE_MODE_H__

#include <stdbool.h>

/** Check the controllers once. Call right after joypad_init(). */
void safe_mode_detect (void);

/** True if the menu was started in safe mode. */
bool safe_mode_active (void);

#endif /* SAFE_MODE_H__ */
