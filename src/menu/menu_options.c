#include <libdragon.h>
#include <stdio.h>

#include "ini_parser.h"
#include "menu_options.h"
#include "profiles.h"
#include "safe_file.h"

#define OPTIONS_PATH        profiles_file("options.ini")   /* the playing profile's */
#define OPTIONS_TMP_PATH    profiles_file("options.tmp")
#define SHARED_PATH         "sd:/menu/options.ini"         /* the first profile's, which also holds the console's own */
#define SHARED_TMP_PATH     "sd:/menu/options.tmp"

typedef struct {
    const char *key;
    int default_value;
} option_info_t;

static const option_info_t option_info[OPTION_COUNT] = {
    [OPTION_SORT_ORDER] = { "sort_order", 0 },
    [OPTION_FONT] = { "font", 0 },
    /* Not "frame_rate": that key was written while the setting was on trial, and
       a leftover 60 must not stay switched on now that the row is hidden. */
    [OPTION_FRAME_RATE] = { "frame_rate_experiment", 0 },
    [OPTION_INTRO] = { "intro", 1 },    /* INTRO_ON: at power-on */
    [OPTION_FADE] = { "fade", 2 },      /* FADE_2_SECONDS */
    [OPTION_THEME] = { "theme", 0 },            /* from the SD card, or Midnight */
    [OPTION_INTRO_LOGO] = { "intro_logo", 0 },  /* vaporwave */
    [OPTION_TAB1] = { "tab1", 1 },      /* Games */
    [OPTION_TAB2] = { "tab2", 2 },      /* Favorites */
    [OPTION_TAB3] = { "tab3", 3 },      /* Folders */
    [OPTION_TAB4] = { "tab4", 0 },      /* nothing; Recent is the player's to add */
    [OPTION_RING_TINT] = { "ring_tint", 1 },    /* on the Game info screen */
    [OPTION_PERF_OVERLAY] = { "perf_overlay", 0 },
    [OPTION_PAK_BADGES] = { "pak_badges", 2 },  /* colored */
    [OPTION_SELECTION] = { "selection", 1 },    /* corners */
    [OPTION_BOX_HOVER] = { "box_hover", 3 },    /* float and sway */
    [OPTION_COVER_STYLE] = { "cover_style", 0 }, /* boxes */
    [OPTION_GAME_VERSIONS] = { "game_versions", 0 }, /* separate */
    [OPTION_REGION_COPIES] = { "region_copies", 0 }, /* separate */
    [OPTION_PREFERRED_REGION] = { "preferred_region", 0 }, /* USA */
    [OPTION_SHOT_SIZE] = { "shot_size", 0 }, /* full */
    [OPTION_SHOT_DITHER] = { "shot_dither", 0 }, /* smooth */
    [OPTION_SHOT_BUTTON] = { "shot_button", 0 }, /* hold START */
    [OPTION_CLIP_LENGTH] = { "clip_length", 1 }, /* 10 seconds */
    [OPTION_CLIP_SIZE] = { "clip_size", 1 }, /* half */
    [OPTION_CLIP_FPS] = { "clip_fps", 0 }, /* 15 a second */
    [OPTION_CLIP_DITHER] = { "clip_dither", 0 }, /* smooth */
    [OPTION_HOMEBREW_ON_GAMES] = { "homebrew_on_games", 0 }, /* show */
    [OPTION_HACKS_ON_GAMES] = { "hacks_on_games", 0 }, /* show */
    [OPTION_THEME_KEY] = { "theme_key", 0 },
    [OPTION_MUSIC_VOLUME] = { "music_volume", 5 }, /* the stock level */
    [OPTION_SOUND_VOLUME] = { "sound_volume", 5 },
    [OPTION_PARTY_TEST] = { "party_test", 0 }, /* count the real controllers */
};

static int values[OPTION_COUNT];
static bool loaded = false;
static int loaded_for = 0;      /* profiles_generation() when loaded */
static bool dirty = false;
static bool shared_dirty = false;

/* Options that belong to the console, not the player: the same for every
   profile, kept in the first profile's file. Character Set applies at a
   restart, before anyone is asked who is playing. */
static bool shared (int option) {
    return option == OPTION_FONT || option == OPTION_FRAME_RATE || option == OPTION_PARTY_TEST;
}

static ini_t *shared_load (void) {
    ini_t *ini = ini_load(SHARED_PATH);
    return ini ? ini : ini_try_load(SHARED_TMP_PATH);
}

static void load (void) {
    if (loaded && loaded_for == profiles_generation()) {
        return;
    }
    loaded = true;
    loaded_for = profiles_generation();     /* another player: their own choices */

    /* If power was cut between remove and rename, the temp file is the good copy. */
    ini_t *ini = ini_load(OPTIONS_PATH);
    if (!ini) {
        ini = ini_try_load(OPTIONS_TMP_PATH);
    }
    for (int i = 0; i < OPTION_COUNT; i++) {
        values[i] = ini_get_int(ini, "options", option_info[i].key, option_info[i].default_value);
    }
    ini_free(ini);
    if (profiles_current() > 0) {
        ini_t *first = shared_load();
        for (int i = 0; i < OPTION_COUNT; i++) {
            if (shared(i)) {
                values[i] = ini_get_int(first, "options", option_info[i].key, option_info[i].default_value);
            }
        }
        ini_free(first);
    }
}

int options_get (option_t option) {
    if (option < 0 || option >= OPTION_COUNT) {
        return 0;
    }
    load();
    return values[option];
}

void options_change (option_t option, int value) {
    if (option < 0 || option >= OPTION_COUNT) {
        return;
    }
    load();
    values[option] = value;
    dirty = true;
    if (shared(option) && profiles_current() > 0) {
        shared_dirty = true;
    }
}

void options_set (option_t option, int value) {
    options_change(option, value);
    options_flush();
}

/* Written to a temporary file first, so a power cut can't leave half a file. */
void options_flush (void) {
    if (!dirty) {
        return;
    }
    dirty = false;

    ini_t *ini = ini_create();
    for (int i = 0; i < OPTION_COUNT; i++) {
        ini_set_int(ini, "options", option_info[i].key, values[i]);
    }
    if (ini_save(ini, OPTIONS_TMP_PATH)) {
        if (!safe_file_replace(OPTIONS_TMP_PATH, OPTIONS_PATH)) {
            debugf("options: could not replace %s\n", OPTIONS_PATH);
        }
    } else {
        debugf("options: could not write %s\n", OPTIONS_TMP_PATH);
    }
    ini_free(ini);

    /* A console option changed by another player: into the first profile's file too. */
    if (shared_dirty) {
        shared_dirty = false;
        ini_t *first = shared_load();
        if (!first) {
            first = ini_create();
        }
        for (int i = 0; i < OPTION_COUNT; i++) {
            if (shared(i)) {
                ini_set_int(first, "options", option_info[i].key, values[i]);
            }
        }
        if (!ini_save(first, SHARED_TMP_PATH) || !safe_file_replace(SHARED_TMP_PATH, SHARED_PATH)) {
            debugf("options: could not write %s\n", SHARED_PATH);
        }
        ini_free(first);
    }
}
