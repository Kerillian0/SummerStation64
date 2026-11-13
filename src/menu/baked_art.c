/**
 * @file baked_art.c
 * @brief Box art baked by make_baked_art.py, read from one file on the SD
 *        card (sd:/menu/art.pak, made by make_art_pack.py).
 *
 * The art used to be inside the menu file, but the SummerCart64 must load
 * that whole file within 5 seconds and 32 MB of art made slow cards miss it
 * ("Watchdog timeout"). Now the pack's index is copied into spare cart
 * memory at the first lookup, and each picture is read on demand into
 * another spare spot there: a read into cart memory goes at the card's full
 * speed, a read into the console's own memory at about 1 KB a millisecond.
 * The pack's place on the card is mapped once, so a picture is read by its
 * sectors with no walk through the card's file table, and pictures are
 * stored unpacked (unpacking cost 6-9 ms a cover). SummerCart64 only; other
 * carts use the card's PNGs.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>
#include <libcart/cart.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>
#include <malloc.h>

#include "memory_note.h"
#include "utils/fs.h"
#include "baked_art.h"

#define MAX_LOADED      (16)            /* more than the cover row and the selected box's edges ever hold at once */
#define MIN_FREE        (160 * 1024)    /* don't load one when memory is this short */

#define PACK_PATH       "sd:/menu/art.pak"
#define PACK_VERSION    (2)     /* 1 held the pictures packed */
#define PACK_RECORD     (16)
/* Spare cart memory, well above the menu file (about 3 MB) and below the
   64DD's start-up program (0x13BC0000); the saves sit at the very top.
   Nothing else uses it while the menu runs; a game loaded over it is
   started straight away, and the next menu start reads the index again. */
#define PACK_INDEX_PI   (0x13800000UL)
#define PACK_INDEX_MAX  (256 * 1024)
#define PACK_STAGE_PI   (PACK_INDEX_PI + PACK_INDEX_MAX)
#define PACK_STAGE_MAX  (256 * 1024)
#define SECTOR          (512)
#define MAX_RUNS        (64)    /* pieces the pack may lie in on the card; more and it is read the slow way */

/* Which covers came from here, and the picture each one's pixels belong to. */
static struct {
    component_boxart_t *art;
    sprite_t *sprite;
} loaded[MAX_LOADED];

static bool code_is_plain (const char *code, int length) {
    for (int i = 0; i < length; i++) {
        if (!isalnum((unsigned char) code[i])) {
            return false;
        }
    }
    return true;
}

/* A European country's letter in a game code. */
static bool is_europe (char letter) {
    return strchr("PDFISUXYHWL", toupper((unsigned char) letter)) != NULL;
}

static enum { PACK_UNTRIED, PACK_OPEN, PACK_NONE } pack_state;
static FIL pack;
static uint32_t pack_count;

/* Where the pack lies on the card: runs of clusters one after another, so
   a picture is read by its sectors without walking the card's file table
   (f_lseek: up to 7 ms a picture). */
static struct {
    uint32_t first;     /* the pack's cluster number where the run starts (0 = its first) */
    uint32_t count;     /* clusters */
    uint32_t sector;    /* the card sector the run starts at */
} runs[MAX_RUNS];
static int run_count;   /* 0: not mapped, read through f_lseek + f_read */

static uint32_t pad_sector (uint32_t n) {
    return (n + SECTOR - 1) & ~(uint32_t) (SECTOR - 1);
}

/* Open the pack and copy its index into cart memory, once per start. */
static bool pack_ready (void) {
    if (pack_state != PACK_UNTRIED) {
        return pack_state == PACK_OPEN;
    }
    pack_state = PACK_NONE;
    if (cart_type != CART_SC) {
        return false;
    }
    uint64_t began = get_ticks_ms();
    if (f_open(&pack, strip_fs_prefix(PACK_PATH), FA_READ) != FR_OK) {
        debugf("art pack: none at %s, covers come from the card's pictures\n", PACK_PATH);
        return false;
    }
    uint8_t header[SECTOR] __attribute__((aligned(16)));
    UINT got = 0;
    if (f_read(&pack, header, SECTOR, &got) != FR_OK || got != SECTOR || memcmp(header, "SS64ART", 8) != 0) {
        debugf("art pack: %s is not an art pack\n", PACK_PATH);
        f_close(&pack);
        return false;
    }
    uint32_t number[4];
    memcpy(number, header + 8, sizeof(number));     /* big-endian, as the console reads it */
    uint32_t version = number[0], count = number[1], index_at = number[2], record = number[3];
    uint32_t index_size = pad_sector(count * PACK_RECORD);
    if (version != PACK_VERSION || record != PACK_RECORD || index_at != SECTOR
            || count == 0 || index_size > PACK_INDEX_MAX) {
        debugf("art pack: version %d with %d pictures is not one this menu reads\n", (int) version, (int) count);
        f_close(&pack);
        return false;
    }
    if (f_read(&pack, (void *) PACK_INDEX_PI, index_size, &got) != FR_OK || got != index_size) {
        debugf("art pack: could not read its index\n");
        f_close(&pack);
        return false;
    }
    pack_count = count;

    /* Map the pack's clusters once: moving forward through a file one
       cluster at a time only reads the next link of the card's table. */
    FATFS *fs = pack.obj.fs;
    uint32_t cluster_bytes = (uint32_t) fs->csize * SECTOR;
    uint32_t clusters = (f_size(&pack) + cluster_bytes - 1) / cluster_bytes;
    run_count = 0;
    for (uint32_t c = 0; c < clusters; c++) {
        if (f_lseek(&pack, c * cluster_bytes + SECTOR / 2) != FR_OK || pack.clust < 2 || pack.clust >= fs->n_fatent) {
            run_count = 0;
            break;
        }
        uint32_t sector = fs->database + (uint32_t) fs->csize * (pack.clust - 2);
        if (run_count > 0 && runs[run_count - 1].sector + runs[run_count - 1].count * fs->csize == sector) {
            runs[run_count - 1].count++;
        } else if (run_count < MAX_RUNS) {
            runs[run_count].first = c;
            runs[run_count].count = 1;
            runs[run_count].sector = sector;
            run_count++;
        } else {
            run_count = 0;  /* in too many pieces: read the slow way */
            break;
        }
    }
    pack_state = PACK_OPEN;
    debugf("art pack: %d pictures, %d MB, index read and mapped in %d ms, %d piece(s) on the card%s\n", (int) count,
        (int) (f_size(&pack) / (1024 * 1024)), (int) (get_ticks_ms() - began), run_count,
        run_count ? "" : " (too many: read the slower way; copying it to the card again helps)");
    return true;
}

/* Find a picture in the pack's index (sorted by its 8-byte key). */
static bool pack_find (const char *code, int length, char side, uint32_t *offset, uint32_t *size) {
    if (!pack_ready()) {
        return false;
    }
    uint8_t key[8] = { 0 };
    memcpy(key, code, length);
    key[4] = (uint8_t) side;
    uint32_t want_hi = ((uint32_t) key[0] << 24) | (key[1] << 16) | (key[2] << 8) | key[3];
    uint32_t want_lo = ((uint32_t) key[4] << 24);
    int low = 0, high = (int) pack_count - 1;
    while (low <= high) {
        int middle = (low + high) / 2;
        uint32_t at = PACK_INDEX_PI + (uint32_t) middle * PACK_RECORD;
        uint32_t hi = io_read(at), lo = io_read(at + 4);
        if (hi == want_hi && lo == want_lo) {
            *offset = io_read(at + 8);
            *size = io_read(at + 12);
            return true;
        }
        if (hi < want_hi || (hi == want_hi && lo < want_lo)) {
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    return false;
}

/* Where a game's baked picture is. @return false if none is baked in. */
/* Read `bytes` (whole sectors) from `offset` in the pack into the stage. */
static bool pack_read (uint32_t offset, uint32_t bytes) {
    if (run_count == 0) {
        UINT got = 0;
        return f_lseek(&pack, offset) == FR_OK
            && f_read(&pack, (void *) PACK_STAGE_PI, bytes, &got) == FR_OK && got == bytes;
    }
    FATFS *fs = pack.obj.fs;
    uint32_t sector = offset / SECTOR, left = bytes / SECTOR, to = PACK_STAGE_PI;
    while (left > 0) {
        uint32_t cluster = sector / fs->csize;
        int r = 0;
        while (r < run_count && !(cluster >= runs[r].first && cluster < runs[r].first + runs[r].count)) {
            r++;
        }
        if (r == run_count) {
            return false;
        }
        uint32_t into = sector - runs[r].first * fs->csize;     /* sectors into the run */
        uint32_t here = runs[r].count * fs->csize - into;      /* sectors left in it */
        uint32_t n = (left < here) ? left : here;
        if (disk_read(fs->pdrv, (BYTE *) to, runs[r].sector + into, n) != RES_OK) {
            return false;
        }
        sector += n;
        left -= n;
        to += n * SECTOR;
    }
    return true;
}

static bool baked_find (const char *game_code, file_image_type_t which, uint32_t *offset, uint32_t *size, char *name) {
    if (!game_code || !code_is_plain(game_code, 4)) {
        return false;
    }
    char side;
    switch (which) {
        case IMAGE_BOXART_FRONT: side = 'f'; break;
        case IMAGE_BOXART_BACK: side = 'b'; break;
        case IMAGE_BOXART_TOP: side = 't'; break;
        case IMAGE_BOXART_LEFT: side = 'l'; break;
        case IMAGE_BOXART_RIGHT: side = 'r'; break;
        case IMAGE_GAMEPAK_FRONT: side = 'p'; break;
        default: return false;
    }

    /* With the region letter first, then (in Europe) the European one,
       then without it: a cover without the letter is baked only for codes
       that have no covers for single regions. */
    char code[4] = { game_code[0], game_code[1], game_code[2], game_code[3] };
    int length = 4;
    bool found = pack_find(code, 4, side, offset, size);
    if (!found && is_europe(game_code[3])) {
        /* Another European country: the pack's European cover. */
        code[3] = 'P';
        found = pack_find(code, 4, side, offset, size);
    }
    if (!found) {
        length = 3;
        found = pack_find(code, 3, side, offset, size);
    }
    if (found && name) {
        snprintf(name, 8, "%.*s_%c", length, code, side);
    }
    return found;
}

bool baked_art_has (const char *game_code, file_image_type_t which) {
    uint32_t offset, size;
    return baked_find(game_code, which, &offset, &size, NULL);
}

component_boxart_t *baked_art_load (const char *game_code, file_image_type_t which) {
    uint32_t offset, size;
    char name[8];
    if (!baked_find(game_code, which, &offset, &size, name) || size == 0 || pad_sector(size) > PACK_STAGE_MAX) {
        return NULL;
    }

    int slot = -1;
    for (int i = 0; i < MAX_LOADED; i++) {
        if (!loaded[i].art) {
            slot = i;
            break;
        }
    }

    /* The loader stops the whole menu if it can't get memory, so make sure
       beforehand that there is room. */
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    if (slot < 0) {
        return NULL;
    }
    if ((heap.total - heap.used) < MIN_FREE) {
        memory_note_cut(MEMORY_CUT_BAKED_ART);
        return NULL;
    }

    component_boxart_t *art = calloc(1, sizeof(component_boxart_t));
    surface_t *image = malloc(sizeof(surface_t));
    if (!art || !image) {
        free(art);
        free(image);
        return NULL;
    }

    /* Into cart memory at the card's full speed (whole sectors: the pack
       keeps every picture on a sector boundary), then copied in. */
    uint64_t began = get_ticks_us();
    uint32_t padded = pad_sector(size);
    void *buffer = memalign(16, padded);
    if (!buffer) {
        free(art);
        free(image);
        return NULL;
    }
    bool read_ok = pack_read(offset, padded);
    uint64_t read_done = get_ticks_us();
    if (read_ok) {
        data_cache_hit_invalidate(buffer, padded);
        dma_read(buffer, PACK_STAGE_PI, padded);
    }
    if (!read_ok) {
        debugf("art pack: could not read %s\n", name);
        free(buffer);
        free(art);
        free(image);
        return NULL;
    }
    sprite_t *sprite = sprite_load_buf(buffer, size);
    sprite->flags |= SPRITE_FLAGS_OWNEDBUFFER;  /* sprite_free frees the buffer */
    debugf("Baked art: %s in %d ms (read %d KB in %d, copy %d)\n", name,
        (int) ((get_ticks_us() - began) / 1000), (int) (size / 1024),
        (int) ((read_done - began) / 1000), (int) ((get_ticks_us() - read_done) / 1000));
    *image = sprite_get_pixels(sprite);
    art->loading = false;
    art->image = image;

    loaded[slot].art = art;
    loaded[slot].sprite = sprite;
    return art;
}

bool baked_art_owns (component_boxart_t *art) {
    for (int i = 0; art && i < MAX_LOADED; i++) {
        if (loaded[i].art == art) {
            return true;
        }
    }
    return false;
}

bool baked_art_free (component_boxart_t *art) {
    if (!art) {
        return false;
    }
    for (int i = 0; i < MAX_LOADED; i++) {
        if (loaded[i].art == art) {
            rspq_wait();    /* nothing may still be drawing from it */
            sprite_free(loaded[i].sprite);
            free(art->image);
            free(art);
            loaded[i].art = NULL;
            loaded[i].sprite = NULL;
            return true;
        }
    }
    return false;
}

bool baked_art_card_ok (const char *storage_prefix, const char *game_code) {
    if (!game_code || (game_code[1] == 'E' && game_code[2] == 'D') || is_europe(game_code[3])) {
        return true;    /* homebrew goes by its title; Europe may use the cover without a letter */
    }
    char folder[64];
    snprintf(folder, sizeof(folder), "%smenu/metadata/%c/%c/%c/%c", storage_prefix, game_code[0], game_code[1], game_code[2], game_code[3]);
    if (directory_exists(folder)) {
        return true;    /* the game has its own */
    }
    folder[strlen(folder) - 2] = '\0';  /* the folder without the region letter */
    if (!directory_exists(folder)) {
        return true;
    }
    /* Covers for single regions beside it: then it is another region's cover. */
    bool ok = true;
    dir_t info;
    int result = dir_findfirst(folder, &info);
    while (result == 0) {
        if (info.d_type == DT_DIR && strlen(info.d_name) == 1) {
            ok = false;
            break;
        }
        result = dir_findnext(folder, &info);
    }
    return ok;
}

component_boxart_t *baked_art_open (const char *storage_prefix, const char *game_code, const char *title, file_image_type_t which) {
    component_boxart_t *art = baked_art_load(game_code, which);
    if (!art && baked_art_card_ok(storage_prefix, game_code)) {
        art = ui_components_boxart_init(storage_prefix, game_code, title, which);
    }
    return art;
}

void baked_art_release (component_boxart_t *art) {
    if (art && !baked_art_free(art)) {
        ui_components_boxart_free(art);
    }
}
