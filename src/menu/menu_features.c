#include <libdragon.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "ini_parser.h"
#include "menu_features.h"
#include "theme.h"

typedef struct {
    const char *key;
    bool default_on;
    bool needs_expansion;
} feature_info_t;

static const feature_info_t feature_info[FEATURE_COUNT] = {
    [FEATURE_QUICK_LAUNCH]        = { "quick_launch",        false, false },
    [FEATURE_BOOT_ANIMATION]      = { "boot_animation",      true,  false },
    [FEATURE_CAROUSEL_ANIMATION]  = { "carousel_animation",  true,  false },
    [FEATURE_RUMBLE]              = { "rumble",              false, false },
    [FEATURE_MUSIC]               = { "music",               false, false },
    [FEATURE_CLOCK]               = { "clock",               true,  false },
    [FEATURE_ATTRACT_MODE]        = { "attract_mode",        false, false },
    [FEATURE_PREBUILT_BACKGROUND] = { "prebuilt_background", true,  true  },
    [FEATURE_SIDE_COVERS]         = { "side_covers",         false, false },
    [FEATURE_FRAME_BORDERS]       = { "frame_borders",       true,  false },
    [FEATURE_UPDOWN_SCROLL]       = { "updown_scroll",       false, false },
};

#define FEATURES_USER_PATH      "sd:/menu/features.ini"
#define FEATURES_USER_TMP_PATH  "sd:/menu/features.tmp"

static int8_t user[FEATURE_COUNT];
static bool user_loaded = false;

static bool detected = false;
static bool expansion_pak = false;

static void features_detect (void) {
    if (!detected) {
        detected = true;
        expansion_pak = is_memory_expanded();
        debugf("features: Expansion Pak %s\n", expansion_pak ? "detected (8MB)" : "not found (4MB)");
    }
}

const char *feature_key (feature_t feature) {
    if (feature < 0 || feature >= FEATURE_COUNT) {
        return "";
    }
    return feature_info[feature].key;
}

feature_t feature_from_key (const char *key) {
    for (int i = 0; i < FEATURE_COUNT; i++) {
        if (!strcasecmp(key, feature_info[i].key)) {
            return (feature_t) i;
        }
    }
    return FEATURE_COUNT;
}

bool features_expansion_pak (void) {
    features_detect();
    return expansion_pak;
}

bool features_available (feature_t feature) {
    if (feature < 0 || feature >= FEATURE_COUNT) {
        return false;
    }
    return !feature_info[feature].needs_expansion || features_expansion_pak();
}

bool features_enabled (feature_t feature) {
    if (!features_available(feature)) {
        return false;
    }

    bool on = feature_info[feature].default_on;

    int theme_value = theme_get()->features[feature];
    if (theme_value != FEATURE_UNSET) {
        on = (theme_value != 0);
    }

    int user_value = features_user_get(feature);
    if (user_value != FEATURE_UNSET) {
        on = (user_value != 0);
    }

    return on;
}

static void features_user_load (void) {
    if (user_loaded) {
        return;
    }
    user_loaded = true;

    /* If power was cut between remove and rename, the temp file is the good copy. */
    ini_t *ini = ini_load(FEATURES_USER_PATH);
    if (!ini) {
        ini = ini_try_load(FEATURES_USER_TMP_PATH);
    }
    for (int i = 0; i < FEATURE_COUNT; i++) {
        int value = ini_get_int(ini, "features", feature_info[i].key, FEATURE_UNSET);
        user[i] = (value == FEATURE_UNSET) ? FEATURE_UNSET : (value != 0);
    }
    ini_free(ini);
}

/* Written to a temporary file first, so a power cut can't leave half a file. */
static void features_user_save (void) {
    ini_t *ini = ini_create();
    bool any = false;
    for (int i = 0; i < FEATURE_COUNT; i++) {
        if (user[i] != FEATURE_UNSET) {
            ini_set_int(ini, "features", feature_info[i].key, user[i]);
            any = true;
        }
    }

    if (!any) {
        remove(FEATURES_USER_PATH);
    } else if (ini_save(ini, FEATURES_USER_TMP_PATH)) {
        remove(FEATURES_USER_PATH);
        if (rename(FEATURES_USER_TMP_PATH, FEATURES_USER_PATH) != 0) {
            debugf("features: could not replace %s\n", FEATURES_USER_PATH);
        }
    } else {
        debugf("features: could not write %s\n", FEATURES_USER_TMP_PATH);
    }

    ini_free(ini);
}

int features_user_get (feature_t feature) {
    if (feature < 0 || feature >= FEATURE_COUNT) {
        return FEATURE_UNSET;
    }
    features_user_load();
    return user[feature];
}

void features_user_set (feature_t feature, int value) {
    if (feature < 0 || feature >= FEATURE_COUNT) {
        return;
    }
    features_user_load();
    user[feature] = (value == FEATURE_UNSET) ? FEATURE_UNSET : (value != 0);
    features_user_save();
}
