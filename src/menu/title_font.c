#include <string.h>

#include <libdragon.h>

#include "fonts.h"
#include "theme.h"
#include "title_font.h"

#define FIT_MARGIN  (16)

static bool loaded = false;

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
    load(FNT_TITLE, "rom:/Firple-Bold-Title.font64");
    load(FNT_TITLE_MEDIUM, "rom:/Firple-Bold-Title20.font64");
    load(FNT_SMALL, "rom:/Firple-Bold-Small.font64");
    loaded = true;
}

/* True if every character is one the title font has: printable ASCII,
   Latin-1 letters and symbols (U+00A1 to U+00FF), or the ellipsis. */
static bool covers (const char *text) {
    const unsigned char *c = (const unsigned char *) text;

    while (*c) {
        if (*c >= 0x20 && *c < 0x7F) {
            c += 1;
        } else if ((*c == 0xC2 && c[1] >= 0xA1 && c[1] <= 0xBF) || (*c == 0xC3 && c[1] >= 0x80 && c[1] <= 0xBF)) {
            c += 2;     /* U+00A1 to U+00FF as two UTF-8 bytes */
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
    if (fits(FNT_TITLE_MEDIUM, text, max_width)) {
        return FNT_TITLE_MEDIUM;
    }
    return FNT_DEFAULT;
}
