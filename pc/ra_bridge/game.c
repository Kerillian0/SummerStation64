/**
 * @file game.c
 * @brief Which game is running, and its RetroAchievements hash (see game.h).
 */

#include <stdio.h>
#include <string.h>

#include "game.h"
#include "sc64_link.h"
#include "rcheevos/src/util/md5.h"

#define NOTE        (0x03B02000UL)      /* RA_HOOK_GAME as the cart's own (USB) address */
#define ROM         (0x00000000UL)      /* the ROM sits at the start of the cart's memory */
#define MAX_ROM     (0x03B00000UL)      /* past this the hook's own area begins */
#define PIECE       (256 * 1024)

static uint32_t get32 (const uint8_t *p) {
    return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | p[3];
}

bool game_read_note (game_note_t *note) {
    uint8_t raw[256];
    memset(note, 0, sizeof(*note));
    if (!sc64_read(NOTE, raw, sizeof(raw)) || memcmp(raw, "RAG1", 4) != 0) {
        return false;
    }
    note->rom_size = get32(raw + 4);
    memcpy(note->code, raw + 8, 4);
    memcpy(note->path, raw + 12, sizeof(note->path) - 1);
    return note->rom_size > 0 && note->rom_size <= MAX_ROM;
}

bool game_hash (const game_note_t *note, char hash[33]) {
    static uint8_t piece[PIECE];
    md5_state_t md5;
    md5_init(&md5);
    for (uint32_t done = 0; done < note->rom_size; ) {
        uint32_t length = note->rom_size - done;
        if (length > PIECE) {
            length = PIECE;
        }
        if (!sc64_read(ROM + done, piece, length)) {
            return false;
        }
        md5_append(&md5, piece, (int) length);
        done += length;
    }
    md5_byte_t digest[16];
    md5_finish(&md5, digest);
    for (int i = 0; i < 16; i++) {
        snprintf(hash + i * 2, 3, "%02x", digest[i]);
    }
    return true;
}
