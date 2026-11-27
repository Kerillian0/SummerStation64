#include <string.h>

#include <libdragon.h>

#include "fonts.h"
#include "theme.h"
#include "title_font.h"
#include "memory_log.h"

#define FIT_MARGIN  (16)

static bool loaded = false;
static bool medium = false;     /* the 20 px size is loaded (Expansion Pak) */

static rdpq_font_t *fonts[3];
static int font_count = 0;

static void load (uint8_t id, const char *path) {
    rdpq_font_t *font = rdpq_font_load(path);
    if (font_count < 3) {
        fonts[font_count++] = font;
    }

    const theme_t *theme = theme_get();
    rdpq_font_style(font, STL_DEFAULT, &((rdpq_fontstyle_t) { .color = theme->text }));
    rdpq_font_style(font, STL_GRAY, &((rdpq_fontstyle_t) { .color = theme->text_dim }));
    rdpq_font_style(font, STL_GREEN, &((rdpq_fontstyle_t) { .color = RGBA32(0x70, 0xFF, 0x70, 0xFF) }));

    rdpq_text_register_font(id, font);
}

void title_font_restyle (void) {
    const theme_t *theme = theme_get();
    for (int i = 0; i < font_count; i++) {
        rdpq_font_style(fonts[i], STL_DEFAULT, &((rdpq_fontstyle_t) { .color = theme->text }));
        rdpq_font_style(fonts[i], STL_GRAY, &((rdpq_fontstyle_t) { .color = theme->text_dim }));
    }
}

void title_font_init (void) {
    int before = memory_log_used();
    load(FNT_TITLE, "rom:/Firple-Bold-Title.font64");
    /* The 20 px size (44 KB) only with the Expansion Pak. Without it, its
       slot points at the body font: a long name goes from 26 px straight to
       15 px, and headings drawn in it come out at the body size. */
    medium = is_memory_expanded();
    if (medium) {
        load(FNT_TITLE_MEDIUM, "rom:/Firple-Bold-Title20.font64");
    } else {
        rdpq_text_register_font(FNT_TITLE_MEDIUM, rdpq_text_get_font(FNT_DEFAULT));
    }
    load(FNT_SMALL, "rom:/Firple-Bold-Small.font64");
    loaded = true;
    memory_log_since(before, "title and small fonts");
}

/* True if every character is one the title font has: printable ASCII,
   Latin-1 letters and symbols (U+00A1 to U+00FF), or the ellipsis. */
static bool covers (const char *text) {
    const unsigned char *c = (const unsigned char *) text;

    while (*c) {
        if (*c >= 0x20 && *c < 0x7F) {
            c += 1;
        } else if ((*c == 0xC2 && (c[1] == 0xA9 || c[1] == 0xAE || c[1] == 0xB0 || c[1] == 0xB7))
                   || (*c == 0xC3 && c[1] >= 0x80 && c[1] <= 0xBF && c[1] != 0x97 && c[1] != 0xB7)) {
            c += 2;     /* © ® ° · and the accented letters (assets/fonts/charset-title.txt), as two UTF-8 bytes */
        } else if (*c == 0xE2 && c[1] == 0x80 && c[2] == 0xA6) {
            c += 3;     /* the ellipsis */
        } else {
            return false;
        }
    }
    return true;
}

static bool fits (uint8_t font, const char *text, int max_width) {
    int nbytes = strlen(text);
    rdpq_paragraph_t *layout = rdpq_paragraph_build(&(rdpq_textparms_t) {0}, font, text, &nbytes);
    int width = (int) (layout->bbox.x1 - layout->bbox.x0);
    rdpq_paragraph_free(layout);

    /* The measurement covers the visible ink only. The text drawer also
       counts the space after the last character, and cuts the text short
       with "..." if that doesn't fit, so leave a character's width spare. */
    return (width + FIT_MARGIN) <= max_width;
}

uint8_t title_font_pick (const char *text, int max_width) {
    if (!loaded || !covers(text)) {
        return FNT_DEFAULT;
    }
    if (fits(FNT_TITLE, text, max_width)) {
        return FNT_TITLE;
    }
    if (medium && fits(FNT_TITLE_MEDIUM, text, max_width)) {
        return FNT_TITLE_MEDIUM;
    }
    return FNT_DEFAULT;
}
