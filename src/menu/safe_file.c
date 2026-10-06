#include <fatfs/ff.h>

#include "safe_file.h"
#include "utils/fs.h"

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

void safe_file_remove (const char *temp_path, const char *final_path) {
    f_unlink(strip_fs_prefix((char *) final_path));
    f_unlink(strip_fs_prefix((char *) temp_path));
}
