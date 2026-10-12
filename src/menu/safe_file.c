#include <fatfs/ff.h>

#include <libdragon.h>

#include "safe_file.h"
#include "utils/fs.h"

#define IDLE_TURN_GAP_MS    (1000)

static uint64_t idle_turn_at = 0;

bool safe_file_replace (const char *temp_path, const char *final_path) {
    /* The C library's rename() isn't wired up for the SD card here, so talk
       to the FAT library directly, as the flashcart code does. */
    char *from = strip_fs_prefix((char *) temp_path);
    char *to = strip_fs_prefix((char *) final_path);

    FRESULT removed = f_unlink(to);
    if (removed != FR_OK && removed != FR_NO_FILE) {
        return false;
    }

    return f_rename(from, to) == FR_OK;
}

bool safe_file_idle_turn (void) {
    uint64_t now = get_ticks_ms();
    if (idle_turn_at != 0 && (now - idle_turn_at) < IDLE_TURN_GAP_MS) {
        return false;
    }
    idle_turn_at = now;
    return true;
}

void safe_file_remove (const char *temp_path, const char *final_path) {
    f_unlink(strip_fs_prefix((char *) final_path));
    f_unlink(strip_fs_prefix((char *) temp_path));
}
