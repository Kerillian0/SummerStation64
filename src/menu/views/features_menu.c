#include <stdint.h>

#include "../menu_features.h"
#include "features_menu.h"

/* Only features that already do something are listed here. */

/* The feature and the chosen value are packed into the action argument. */
#define CHOICE(feature, value)  ((void *)(uintptr_t)((((feature) + 1) << 8) | ((value) & 0xFF)))

static void set_feature (menu_t *menu, void *arg) {
    (void)menu;
    uintptr_t packed = (uintptr_t)(arg);
    feature_t feature = (feature_t)((packed >> 8) - 1);
    int value = (int8_t)(packed & 0xFF);
    features_user_set(feature, value);

    // The menu closes after a choice; forget which feature was open so that
    // "Menu Features" shows the feature list again next time.
    features_context_menu.submenu = NULL;
}

/* Row to start on: 0 = Profile Default (follow the theme), 1 = On, 2 = Off. */
static int row_for (feature_t feature) {
    switch (features_user_get(feature)) {
        case FEATURE_UNSET: return 0;
        case 0: return 2;
        default: return 1;
    }
}

#define FEATURE_SUBMENU(name, feature) \
    static int name##_selection (menu_t *menu) { \
        (void)menu; \
        return row_for(feature); \
    } \
    static component_context_menu_t name##_context_menu = { \
        .get_default_selection = name##_selection, \
        .list = { \
            { .text = "Profile Default", .action = set_feature, .arg = CHOICE(feature, FEATURE_UNSET) }, \
            { .text = "On", .action = set_feature, .arg = CHOICE(feature, 1) }, \
            { .text = "Off", .action = set_feature, .arg = CHOICE(feature, 0) }, \
            COMPONENT_CONTEXT_MENU_LIST_END, \
        } \
    }

FEATURE_SUBMENU(side_covers, FEATURE_SIDE_COVERS);
FEATURE_SUBMENU(frame_borders, FEATURE_FRAME_BORDERS);
FEATURE_SUBMENU(updown_scroll, FEATURE_UPDOWN_SCROLL);
FEATURE_SUBMENU(quick_launch, FEATURE_QUICK_LAUNCH);
FEATURE_SUBMENU(hold_launch, FEATURE_HOLD_LAUNCH);

component_context_menu_t features_context_menu = { .list = {
    { .text = "Previous/Next Covers", .submenu = &side_covers_context_menu },
    { .text = "Frame Borders", .submenu = &frame_borders_context_menu },
    { .text = "Up/Down Also Scroll", .submenu = &updown_scroll_context_menu },
    { .text = "Quick Launch", .submenu = &quick_launch_context_menu },
    { .text = "Hold A To Launch", .submenu = &hold_launch_context_menu },
    COMPONENT_CONTEXT_MENU_LIST_END,
}};
