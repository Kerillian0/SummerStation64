/**
 * @file crash_screen.h
 * @brief A plain-language screen shown when the menu crashes.
 *
 * Replaces the wall of registers with a short explanation and a pointer
 * to Safe Mode. The technical screen is still one button press away.
 */

#ifndef CRASH_SCREEN_H__
#define CRASH_SCREEN_H__

/** Install the crash screen. Call once, as early as possible. */
void crash_screen_init (void);

#endif /* CRASH_SCREEN_H__ */
