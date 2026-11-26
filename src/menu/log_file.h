/**
 * @file log_file.h
 * @brief The menu's log, kept in memory, saved to the SD card for a bug report.
 *
 * Everything the menu writes to its debug log (debugf) is also kept in a
 * 8 KB ring in memory, about the last 100 lines. Most players have no
 * USB cable to read the log live, so:
 *   - Settings > Console > About > Save Log writes it to sd:/menu/log.txt.
 *   - When the menu crashes (the friendly crash screen, or a failed check
 *     that shows libdragon's own screen), the ring is copied into spare cart
 *     memory, which keeps it through RESET (not through switching off). At
 *     the next start it is written to sd:/menu/crash-log.txt.
 * The lines are caught by wrapping libdragon's write() (--wrap=write in the
 * Makefile); they still go on to the USB log as before.
 */

#ifndef LOG_FILE_H__
#define LOG_FILE_H__

#include <stdbool.h>

/** Write the kept lines to sd:/menu/log.txt. False if the card refused. */
bool log_file_save (const char *storage_prefix);

/** The menu is crashing: copy the kept lines to cart memory for the next start. Safe with interrupts off. */
void log_file_crash (void);

/** At start, once the SD card works: a crash's lines left in cart memory are saved to sd:/menu/crash-log.txt. */
void log_file_rescue (const char *storage_prefix);

/** True when this start saved a crash log (for a note on the About row). */
bool log_file_crash_saved (void);

#endif /* LOG_FILE_H__ */
