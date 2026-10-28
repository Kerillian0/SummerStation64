#include <libdragon.h>
#include <stdio.h>
#include <string.h>

#include "card_themes.h"
#include "safe_mode.h"
#include "theme_pack.h"
#include "utils/fs.h"

#define WAV64_HEADER_SIZE   (32)
#define LOUDEST_FREQUENCY   (44100)     /* the sound effects' channel plays up to this */
#define FONT_MAX_4MB        (128 * 1024)
#define EFFECT_MAX_4MB      (48 * 1024)     /* each button sound is kept in memory: about 0.7 s at 32 kHz */
#define EFFECT_MAX_8MB      (128 * 1024)
#define FONT_MAX_8MB        (1024 * 1024)
#define INTRO_MAX_4MB       (160 * 1024)    /* the intro tune, kept in memory while it plays: about 1.8 s of 22 kHz stereo */
#define INTRO_MAX_8MB       (768 * 1024)    /* the longest intro (8 s) at 22 kHz stereo */

/* The wav64 reader stops the menu on a file it doesn't like, so look at the
   header first and only hand over files it will take. */
static bool sound_ok (const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    uint8_t h[WAV64_HEADER_SIZE];
    bool read_all = fread(h, 1, sizeof(h), f) == sizeof(h);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    if (!read_all) {
        return false;
    }
    #define BE32(p) ((int32_t) (((uint32_t) (p)[0] << 24) | ((uint32_t) (p)[1] << 16) | ((uint32_t) (p)[2] << 8) | (p)[3]))
    int version = h[4], format = h[5], channels = h[6], bits = h[7];
    int32_t freq = BE32(h + 8), len = BE32(h + 12), loop_len = BE32(h + 16);
    uint32_t start = (uint32_t) BE32(h + 24), state = (uint32_t) BE32(h + 28);
    #undef BE32
    bool good = memcmp(h, "WV64", 4) == 0 && version == 10
        && (format == 0 || format == 1)                 /* raw, or VADPCM (the only packing the menu sets up) */
        && channels >= 1 && channels <= 2
        && (bits == 16 || (bits == 8 && format == 0))
        && freq >= 8000 && freq <= LOUDEST_FREQUENCY
        && len > 0 && loop_len >= 0 && loop_len <= len
        && start >= WAV64_HEADER_SIZE && (long) start < size;
    if (good && format == 0) {
        good = state == 0 && (long) start + (long) len * channels * (bits / 8) <= size;
    }
    if (!good) {
        debugf("theme pack: %s is not a sound the menu can play, using its own\n", path);
    }
    return good;
}

const char *theme_pack_sound (const char *file_name, const char *rom_path) {
    static char path[4][160];   /* a few in use at once (the five effects are opened one after another) */
    static int turn = 0;
    const char *folder = safe_mode_active() ? NULL : card_themes_folder();
    if (!folder) {
        return rom_path;
    }
    char *out = path[turn];
    turn = (turn + 1) % 4;
    snprintf(out, sizeof(path[0]), "%s/%s", folder, file_name);
    if (!file_exists(out) || !sound_ok(out)) {
        return rom_path;
    }
    debugf("theme pack: %s\n", out);
    return out;
}

static long file_size (const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

/* Reading a sound from the card bit by bit while it plays held up frames
   by 50-90 ms and made it crunch (the owner's logs, 2026-10-28), so a
   theme's button sounds and intro tune are read into memory once, whole.
   Too large for this console: the menu's own. */
static wav64_t *load_whole (const char *file_name, const char *rom_path, long most_4mb, long most_8mb) {
    const char *path = theme_pack_sound(file_name, rom_path);
    if (strncmp(path, "rom:", 4) != 0) {
        long most = (get_memory_size() > 0x400000) ? most_8mb : most_4mb;
        long size = file_size(path);
        if (size > 0 && size <= most) {
            wav64_t *wav = wav64_load(path, &(wav64_loadparms_t) { .streaming_mode = WAV64_STREAMING_NONE });
            if (wav) {
                return wav;
            }
        }
        debugf("theme pack: %s left out (%ld bytes, at most %ld kept in memory here)\n", path, size, most);
        path = rom_path;
    }
    return wav64_load(path, NULL);
}

wav64_t *theme_pack_effect (const char *file_name, const char *rom_path) {
    return load_whole(file_name, rom_path, EFFECT_MAX_4MB, EFFECT_MAX_8MB);
}

wav64_t *theme_pack_intro (void) {
    return load_whole("intro.wav64", "rom:/intro.wav64", INTRO_MAX_4MB, INTRO_MAX_8MB);
}

void theme_pack_note_mixing (int ms) {
    static int worst = 0;
    static uint64_t since = 0;
    if (ms > worst) {
        worst = ms;
    }
    uint64_t now = get_ticks_ms();
    if (now - since >= 1000) {
        if (worst >= 8) {
            debugf("sound: mixing took up to %d ms\n", worst);
        }
        worst = 0;
        since = now;
    }
}

const char *theme_pack_font (void) {
    static char path[160];
    const char *folder = safe_mode_active() ? NULL : card_themes_folder();
    if (!folder) {
        return NULL;
    }
    snprintf(path, sizeof(path), "%s/font.font64", folder);
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    /* A font is unpacked into memory, and running out of it stops the menu:
       keep a large one (a full Japanese set) for consoles with the Expansion Pak. */
    long most = (get_memory_size() > 0x400000) ? FONT_MAX_8MB : FONT_MAX_4MB;
    if (size <= 16 || size > most) {
        debugf("theme pack: %s left out (%ld bytes, at most %ld here)\n", path, size, most);
        return NULL;
    }
    debugf("theme pack: font %s\n", path);
    return path;
}
