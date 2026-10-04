/**
 * @file menu_features.h
 * @brief Optional menu features, Expansion Pak detection and toggles.
 *
 * Each feature is decided in layers, the most specific one winning:
 *   1. built-in default
 *   2. the theme's [features] section (theme.ini)
 *   3. the user's own setting (Settings screen, saved to features.ini)
 * Features that need the Expansion Pak are always off without one.
 */

#ifndef MENU_FEATURES_H__
#define MENU_FEATURES_H__

#include <stdbool.h>

typedef enum {
    FEATURE_QUICK_LAUNCH,
    FEATURE_BOOT_ANIMATION,
    FEATURE_CAROUSEL_ANIMATION,
    FEATURE_RUMBLE,
    FEATURE_MUSIC,
    FEATURE_CLOCK,
    FEATURE_ATTRACT_MODE,
    FEATURE_PREBUILT_BACKGROUND,
    FEATURE_SIDE_COVERS,
    FEATURE_FRAME_BORDERS,
    FEATURE_COUNT
} feature_t;

#define FEATURE_UNSET   (-1)

/** Key used for this feature in theme.ini / settings, e.g. "quick_launch". */
const char *feature_key (feature_t feature);

/** Look up a feature by its key. Returns FEATURE_COUNT if unknown. */
feature_t feature_from_key (const char *key);

/** True if an Expansion Pak (8MB) is installed. */
bool features_expansion_pak (void);

/** True if this console can use the feature at all. */
bool features_available (feature_t feature);

/** True if the feature is currently turned on. */
bool features_enabled (feature_t feature);

/** The user's own choice: FEATURE_UNSET (follow the theme), 0 or 1. */
int features_user_get (feature_t feature);

/** Store the user's choice and save it to the SD card. */
void features_user_set (feature_t feature, int value);

#endif /* MENU_FEATURES_H__ */
