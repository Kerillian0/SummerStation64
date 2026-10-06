#include <libdragon.h>

#include "font_choice.h"
#include "menu_features.h"
#include "menu_options.h"

#define FONT_FULL_PATH  "rom:/Firple-Bold.font64"
#define FONT_SMALL_PATH "rom:/Firple-Bold-Latin.font64"

const char *font_choice_name (int choice) {
    switch (choice) {
        case FONT_FULL: return "Full";
        case FONT_SMALL: return "Latin Only";
        default: return "Auto";
    }
}

const char *font_choice_path (void) {
    bool small;

    switch (options_get(OPTION_FONT)) {
        case FONT_FULL:
            small = false;
            break;
        case FONT_SMALL:
            small = true;
            break;
        default:
            small = !features_expansion_pak();
            break;
    }

    debugf("font: %s\n", small ? "small (Latin)" : "full");
    return small ? FONT_SMALL_PATH : FONT_FULL_PATH;
}
