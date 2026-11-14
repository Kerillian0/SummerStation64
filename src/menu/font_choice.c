#include <libdragon.h>

#include "font_choice.h"
#include "menu_features.h"
#include "menu_options.h"
#include "memory_note.h"

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
            /* The full set needs about 700 KB more. Since v0.5 the menu
               itself leaves too little for that on a console without the
               Expansion Pak (2,313 KB of heap, about 1,820 KB used at rest
               with Latin Only, measured 2026-11-14), so it would stop at
               start-up: use Latin Only there, and say so. */
            small = !features_expansion_pak();
            if (small) {
                memory_note_cut(MEMORY_CUT_FULL_FONT);
            }
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
