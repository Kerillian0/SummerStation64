/**
 * @file safe_file.h
 * @brief Finishing a safe file write: swap a freshly written temp file in.
 *
 * Settings files are written to a temp file first and then swapped into
 * place, so switching the console off mid-write can't leave half a file.
 */

#ifndef SAFE_FILE_H__
#define SAFE_FILE_H__

#include <stdbool.h>

/**
 * Replace `final_path` with `temp_path`.
 *
 * If power is lost between the two steps, the temp file is left as the only
 * copy, so loaders should fall back to it when the final file is missing.
 *
 * @return True on success.
 */
bool safe_file_replace (const char *temp_path, const char *final_path);

/** Delete a settings file together with any leftover temp copy of it. */
void safe_file_remove (const char *temp_path, const char *final_path);

#endif /* SAFE_FILE_H__ */
