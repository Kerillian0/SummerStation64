#include <libdragon.h>
#include <stdio.h>

#include "ini_parser.h"
#include "menu_options.h"
#include "safe_file.h"

#define OPTIONS_PATH        "sd:/menu/options.ini"
#define OPTIONS_TMP_PATH    "sd:/menu/options.tmp"

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
    [OPTION_TAB1] = { "tab1", 1 },      /* Games */
    [OPTION_TAB2] = { "tab2", 2 },      /* Favorites */
    [OPTION_TAB3] = { "tab3", 3 },      /* Folders */
    [OPTION_TAB4] = { "tab4", 0 },      /* nothing; Recent is the player's to add */
};

static int values[OPTION_COUNT];
static bool loaded = false;
static bool dirty = false;

static void load (void) {
    if (loaded) {
        return;
    }
    loaded = true;

    /* If power was cut between remove and rename, the temp file is the good copy. */
    ini_t *ini = ini_load(OPTIONS_PATH);
    if (!ini) {
        ini = ini_try_load(OPTIONS_TMP_PATH);
    }
    for (int i = 0; i < OPTION_COUNT; i++) {
        values[i] = ini_get_int(ini, "options", option_info[i].key, option_info[i].default_value);
    }
    ini_free(ini);
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
}
