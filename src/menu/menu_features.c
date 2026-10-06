#include <libdragon.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "ini_parser.h"
#include "menu_features.h"
#include "safe_file.h"
#include "safe_mode.h"
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
    [FEATURE_HOLD_LAUNCH]         = { "hold_launch",         false, false },
    [FEATURE_COVER_ART]           = { "cover_art",           true,  false },
    [FEATURE_SEE_THROUGH_COVERS]  = { "see_through_covers",  true,  false },
    [FEATURE_REMEMBER_SELECTION]  = { "remember_selection",  true,  false },
    [FEATURE_HIDE_EXTENSIONS]     = { "hide_extensions",     true,  false },
    [FEATURE_TIDY_TITLES]         = { "tidy_titles",         true,  false },
    [FEATURE_HIDE_TAGS]           = { "hide_tags",           false, false },
    [FEATURE_REMEMBER_SETTINGS]   = { "remember_settings",   true,  false },
};

#define FEATURES_USER_PATH      "sd:/menu/features.ini"
#define FEATURES_USER_TMP_PATH  "sd:/menu/features.tmp"

static int8_t user[FEATURE_COUNT];
static bool user_loaded = false;
static bool user_dirty = false;

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

bool features_profile_default (feature_t feature) {
    if (!features_available(feature)) {
        return false;
    }

    bool on = feature_info[feature].default_on;

    /* Safe mode: built-in defaults only. */
    if (safe_mode_active()) {
        return on;
    }

    int theme_value = theme_get()->features[feature];
    if (theme_value != FEATURE_UNSET) {
        on = (theme_value != 0);
    }

    return on;
}

bool features_enabled (feature_t feature) {
    if (!features_available(feature)) {
        return false;
    }

    /* The two launch modes can't both be on; Quick Launch wins (a theme
       could still ask for both). */
    if (feature == FEATURE_HOLD_LAUNCH && features_enabled(FEATURE_QUICK_LAUNCH)) {
        return false;
    }

    bool on = features_profile_default(feature);

    /* Safe mode ignores the user's choices too. They can still be changed and
       saved in Settings; they take effect on the next normal start. */
    if (safe_mode_active()) {
        return on;
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
        safe_file_remove(FEATURES_USER_TMP_PATH, FEATURES_USER_PATH);
    } else if (ini_save(ini, FEATURES_USER_TMP_PATH)) {
        if (!safe_file_replace(FEATURES_USER_TMP_PATH, FEATURES_USER_PATH)) {
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

void features_user_change (feature_t feature, int value) {
    if (feature < 0 || feature >= FEATURE_COUNT) {
        return;
    }
    features_user_load();
    user[feature] = (value == FEATURE_UNSET) ? FEATURE_UNSET : (value != 0);

    /* The two launch modes exclude each other: when one ends up on, the
       other is turned off. "Ends up on" includes being set back to Default
       while the theme's default for it is On. */
    bool now_on = (user[feature] != FEATURE_UNSET) ? (user[feature] != 0) : features_profile_default(feature);
    if (now_on && feature == FEATURE_QUICK_LAUNCH) {
        user[FEATURE_HOLD_LAUNCH] = 0;
    } else if (now_on && feature == FEATURE_HOLD_LAUNCH) {
        user[FEATURE_QUICK_LAUNCH] = 0;
    }

    user_dirty = true;
}

void features_user_flush (void) {
    if (user_dirty) {
        user_dirty = false;
        features_user_save();
    }
}

void features_user_set (feature_t feature, int value) {
    features_user_change(feature, value);
    features_user_flush();
}
