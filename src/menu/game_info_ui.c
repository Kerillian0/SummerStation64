#include <stdio.h>
#include <string.h>

#include <libdragon.h>

#include "fonts.h"
#include "game_info_ui.h"
#include "games_ui.h"
#include "path.h"
#include "theme.h"
#include "title_font.h"
#include "ui_components/constants.h"

/* Everything sits between the same edges as the title panel on the Games screen. */
#define X0              (GAMES_UI_CONTENT_X0)
#define X1              (GAMES_UI_CONTENT_X1)

/* Cover, top right */
#define COVER_WIDTH     (176)
#define COVER_HEIGHT    (126)
#define COVER_X         (X1 - COVER_WIDTH)
#define COVER_Y         (44)

/* Title and maker, top left */
#define TITLE_WIDTH     (COVER_X - X0 - 16)
#define MAKER_Y         (44)
#define TITLE_Y         (66)
#define TITLE_HEIGHT    (72)    /* two lines of the big font */
#define BADGES_Y        (146)

/* Three boxes of facts */
#define FACTS_Y         (182)
#define FACTS_HEIGHT    (46)
#define FACTS_GAP       (8)

/* Description */
#define DESC_Y          (FACTS_Y + FACTS_HEIGHT + 8)
#define DESC_HEIGHT     (112)

#define PAD             (10)

static void fill (int x0, int y0, int x1, int y1, color_t color) {
    rdpq_mode_push();
        if (color.a == 0xFF) {
            rdpq_set_mode_fill(color);
        } else {
            rdpq_set_mode_standard();
            rdpq_set_prim_color(color);
            rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        }
        rdpq_fill_rectangle(x0, y0, x1, y1);
    rdpq_mode_pop();
}

static color_t panel_color (void) {
    const theme_t *t = theme_get();
    return RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8);
}

static bool art_ready (component_boxart_t *art) {
    return art && art->image && art->image->width > 0 && art->image->height > 0;
}

/* The backdrop is a tiny copy of the front cover (a quarter of its size each
   way, about 2 KB), kept for as long as this game is shown. It stays the
   front cover while the player looks through the game's other pictures. */
#define BACKDROP_SHRINK (4)

static surface_t backdrop;
static uint32_t backdrop_key = 0;

static uint32_t text_hash (const char *text) {
    uint32_t hash = 2166136261u;
    while (*text) {
        hash = (hash ^ (uint8_t) *text++) * 16777619u;
    }
    return hash ? hash : 1;
}

static void backdrop_make (surface_t *image) {
    if (surface_get_format(image) != FMT_RGBA16) {
        return;
    }

    int w = image->width / BACKDROP_SHRINK;
    int h = image->height / BACKDROP_SHRINK;
    if (w < 1 || h < 1) {
        return;
    }

    backdrop = surface_alloc(FMT_RGBA16, w, h);
    if (!backdrop.buffer) {
        return;
    }

    /* Each small pixel is the average of a 4x4 block of the cover. */
    for (int y = 0; y < h; y++) {
        uint16_t *out = (uint16_t *) ((uint8_t *) backdrop.buffer + y * backdrop.stride);
        for (int x = 0; x < w; x++) {
            int r = 0, g = 0, b = 0;
            for (int sy = 0; sy < BACKDROP_SHRINK; sy++) {
                uint16_t *in = (uint16_t *) ((uint8_t *) image->buffer + (y * BACKDROP_SHRINK + sy) * image->stride);
                for (int sx = 0; sx < BACKDROP_SHRINK; sx++) {
                    uint16_t p = in[x * BACKDROP_SHRINK + sx];
                    r += (p >> 11) & 0x1F;
                    g += (p >> 6) & 0x1F;
                    b += (p >> 1) & 0x1F;
                }
            }
            int n = BACKDROP_SHRINK * BACKDROP_SHRINK;
            out[x] = (uint16_t) (((r / n) << 11) | ((g / n) << 6) | ((b / n) << 1) | 1);
        }
    }
}

/* Keep the backdrop in step with the game being shown. */
static void backdrop_update (menu_t *menu, surface_t *image, bool front_picture) {
    uint32_t key = menu->load.rom_path ? text_hash(path_get(menu->load.rom_path)) : 0;

    if (key != backdrop_key) {
        if (backdrop.buffer) {
            rspq_wait(); /* nothing may still be drawing from it */
            surface_free(&backdrop);
        }
        memset(&backdrop, 0, sizeof(backdrop));
        backdrop_key = key;
    }

    if (!backdrop.buffer && image && front_picture) {
        backdrop_make(image);
    }
}

/* The backdrop stretched over the whole screen. Stretching so small a
   picture this far with smoothing blurs it, and drawing it at a third of its
   brightness keeps the text on top readable. */
static void draw_backdrop (surface_t *image) {
    float scale_x = (float) DISPLAY_WIDTH / image->width;
    float scale_y = (float) DISPLAY_HEIGHT / image->height;
    float scale = (scale_x > scale_y) ? scale_x : scale_y;
    int w = (int) (image->width * scale);
    int h = (int) (image->height * scale);

    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_set_prim_color(RGBA32(0x58, 0x58, 0x58, 0xFF));
        rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
        rdpq_tex_blit(image, (DISPLAY_WIDTH - w) / 2, (DISPLAY_HEIGHT - h) / 2, &(rdpq_blitparms_t) {
            .scale_x = scale,
            .scale_y = scale,
        });
    rdpq_mode_pop();
}

static void draw_cover (surface_t *image) {
    const theme_t *t = theme_get();

    /* Ring in the accent color, as on the Games screen. */
    fill(COVER_X - 3, COVER_Y - 3, COVER_X + COVER_WIDTH + 3, COVER_Y + COVER_HEIGHT + 3, t->accent);
    fill(COVER_X, COVER_Y, COVER_X + COVER_WIDTH, COVER_Y + COVER_HEIGHT, t->panel);

    if (!image) {
        return;
    }

    float scale_x = (float) COVER_WIDTH / image->width;
    float scale_y = (float) COVER_HEIGHT / image->height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    int w = (int) (image->width * scale);
    int h = (int) (image->height * scale);

    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        rdpq_tex_blit(image, COVER_X + (COVER_WIDTH - w) / 2, COVER_Y + (COVER_HEIGHT - h) / 2, &(rdpq_blitparms_t) {
            .scale_x = scale,
            .scale_y = scale,
        });
    rdpq_mode_pop();
}

/* A labelled fact in a dark box: a dim caption with the value under it. */
static void draw_fact (int index, const char *caption, const char *value) {
    int width = (X1 - X0 - (FACTS_GAP * 2)) / 3;
    int x = X0 + index * (width + FACTS_GAP);

    fill(x, FACTS_Y, x + width, FACTS_Y + FACTS_HEIGHT, panel_color());

    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = width - (PAD * 2),
        .height = FACTS_HEIGHT / 2,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES,
        .style_id = STL_GRAY,
    }, FNT_DEFAULT, x + PAD, FACTS_Y + 2, "%s", caption);

    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = width - (PAD * 2),
        .height = FACTS_HEIGHT / 2,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES,
    }, FNT_DEFAULT, x + PAD, FACTS_Y + (FACTS_HEIGHT / 2) - 2, "%s", value);
}

static bool known (const char *text) {
    return text && text[0] != '\0' && strcmp(text, "Not specified") != 0;
}

void game_info_ui_draw (menu_t *menu, component_boxart_t *art, const game_info_view_t *view) {
    rom_info_t *info = &menu->load.rom_info;
    surface_t *image = art_ready(art) ? art->image : NULL;

    backdrop_update(menu, image, view->front_picture);
    if (backdrop.buffer) {
        draw_backdrop(&backdrop);
    }
    draw_cover(image);

    /* Who made it, above the title. */
    if (known(info->meta.author)) {
        rdpq_text_printf(&(rdpq_textparms_t) {
            .width = TITLE_WIDTH,
            .wrap = WRAP_ELLIPSES,
            .style_id = STL_GRAY,
        }, FNT_DEFAULT, X0, MAKER_Y + 14, "%s", info->meta.author);
    }

    /* The name, as big as fits in two lines. */
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = TITLE_WIDTH,
        .height = TITLE_HEIGHT,
        .wrap = WRAP_WORD,
    }, title_font_pick(view->name, (TITLE_WIDTH * 2) - 32), X0, TITLE_Y, "%s", view->name);

    /* What the game supports. */
    int x = X0 - 6;
    if (info->features.expansion_pak == EXPANSION_PAK_REQUIRED) {
        x = games_ui_badge(x, BADGES_Y, "Needs Expansion Pak", STL_ORANGE, true);
    } else if (info->features.expansion_pak == EXPANSION_PAK_RECOMMENDED || info->features.expansion_pak == EXPANSION_PAK_SUGGESTED) {
        x = games_ui_badge(x, BADGES_Y, "Expansion Pak", STL_GREEN, true);
    }
    if (info->features.rumble_pak) {
        x = games_ui_badge(x, BADGES_Y, "Rumble", STL_DEFAULT, true);
    }
    if (info->features.controller_pak && x < COVER_X - 130) {
        x = games_ui_badge(x, BADGES_Y, "Controller Pak", STL_DEFAULT, true);
    }
    if (info->features.transfer_pak && x < COVER_X - 120) {
        x = games_ui_badge(x, BADGES_Y, "Transfer Pak", STL_DEFAULT, true);
    }

    /* The save type is left off this page: few people need it, and it can
       still be seen and changed under Options. */
    char players[16];
    if (info->meta.num_players > 1) {
        snprintf(players, sizeof(players), "1 to %d", (int) info->meta.num_players);
    } else {
        snprintf(players, sizeof(players), "1");
    }
    draw_fact(0, "Players", players);
    draw_fact(1, "TV region", view->tv);
    draw_fact(2, "Released", known(info->meta.release_date) ? info->meta.release_date : "Unknown");

    /* Description, with this game's own switches on the last line. */
    fill(X0, DESC_Y, X1, DESC_Y + DESC_HEIGHT, panel_color());
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = X1 - X0 - (PAD * 2),
        .height = DESC_HEIGHT - 30,
        .wrap = WRAP_WORD,
    }, FNT_DEFAULT, X0 + PAD, DESC_Y + 20, "%s", view->description);
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = X1 - X0 - (PAD * 2),
        .wrap = WRAP_ELLIPSES,
        .style_id = STL_GRAY,
    }, FNT_DEFAULT, X0 + PAD, DESC_Y + DESC_HEIGHT - 8, "Cheats: %s   Patches: %s   Clear memory: %s",
        view->cheats ? "On" : "Off", view->patches ? "On" : "Off", view->clear_rdram ? "On" : "Off");

    /* Button hints, in the same two rows as the Games screen. */
    x = games_ui_hint_draw(GAMES_UI_HINTS_X, 0, "A", "Play");
    games_ui_hint_draw(x, 0, "B", "Back");
    x = games_ui_hint_draw(GAMES_UI_HINTS_X, 1, "R", "Options");
    games_ui_hint_draw(x, 1, "L", "More info");
    games_ui_hint_right_draw(0, "START", "Technical");
    games_ui_hint_right_draw(1, "◀▶", "Pictures");
}
