#include <libdragon.h>
#include <string.h>
#include <strings.h>

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
};

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

    /* User setting override goes here in a later step. */

    return on;
}
