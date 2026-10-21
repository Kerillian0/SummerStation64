#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <libdragon.h>

#include "fonts.h"
#include "game_data.h"
#include "game_info_ui.h"
#include "art_tint.h"
#include "dump_check.h"
#include "games_filter.h"
#include "games_ui.h"
#include "menu_features.h"
#include "path.h"
#include "play_stats.h"
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
#define TITLE_MAIN_HEIGHT   (36)    /* a "Series - Subtitle" name: the series on one big line... */
#define SUBTITLE_Y      (TITLE_Y + 34)
#define SUBTITLE_HEIGHT (40)        /* ...and the subtitle smaller under it */
#define BADGES_Y        (146)

/* Three boxes of facts */
#define FACTS_Y         (182)
#define FACTS_HEIGHT    (46)
#define FACTS_GAP       (8)

/* Description */
#define DESC_Y          (FACTS_Y + FACTS_HEIGHT + 8)
#define DESC_HEIGHT     (140)   /* down to just above the button hints */
#define DESC_SWITCHES   (26)    /* room kept at the bottom when one of the game's switches is on */
#define DESC_NOTE       (46)    /* room for the two-line note about a copy that isn't a known good dump */

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

/* The cover's own color, for the ring round it (option `ring_tint`):
   worked out once per game, when the backdrop is made. */
static color_t ring_tint;
static bool ring_tint_ready = false;

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

    ring_tint_ready = art_tint_from(image, &ring_tint);
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
        ring_tint_ready = false;
    }

    if (!backdrop.buffer && image && front_picture) {
        backdrop_make(image);
    }
}

bool game_info_ui_backdrop_ready (menu_t *menu) {
    return backdrop.buffer && menu->load.rom_path && (backdrop_key == text_hash(path_get(menu->load.rom_path)));
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
    color_t ring = (ring_tint_ready && art_tint_on_game_info()) ? ring_tint : t->accent;

    /* The box takes the art's own shape (a tall Japanese box stays tall,
       with no bars at its sides), kept in the middle of the cover's place. */
    int w = COVER_WIDTH, h = COVER_HEIGHT;
    float scale = 1.0f;
    if (image) {
        float scale_x = (float) COVER_WIDTH / image->width;
        float scale_y = (float) COVER_HEIGHT / image->height;
        scale = (scale_x < scale_y) ? scale_x : scale_y;
        w = (int) (image->width * scale);
        h = (int) (image->height * scale);
    }
    int x = COVER_X + (COVER_WIDTH - w) / 2;
    int y = COVER_Y + (COVER_HEIGHT - h) / 2;

    /* Ring in the cover's own color, or the accent color as on the Games
       screen. */
    fill(x - 3, y - 3, x + w + 3, y + h + 3, ring);
    fill(x, y, x + w, y + h, t->panel);

    if (!image) {
        return;
    }

    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        rdpq_tex_blit(image, x, y, &(rdpq_blitparms_t) {
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

static const char *month_names[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* "Today", "Yesterday", "5 days ago", or the date once it is over a month. */
static void format_last_played (char *out, size_t size, time_t last, time_t now, int count) {
    if (count <= 0) {
        snprintf(out, size, "Never");
    } else if (last <= 0 || now <= 0) {
        snprintf(out, size, "Unknown"); /* no working clock then, or now */
    } else {
        long days = (long) (now / 86400) - (long) (last / 86400);
        if (days <= 0) {
            snprintf(out, size, "Today");
        } else if (days == 1) {
            snprintf(out, size, "Yesterday");
        } else if (days <= 30) {
            snprintf(out, size, "%ld days ago", days);
        } else {
            struct tm *when = localtime(&last);
            if (when) {
                snprintf(out, size, "%s %d, %d", month_names[when->tm_mon % 12], when->tm_mday, when->tm_year + 1900);
            } else {
                snprintf(out, size, "Unknown");
            }
        }
    }
}

/* "2000-10-26" as "Oct 26, 2000" (and "2000-10" as "Oct 2000"). Anything
   written another way is shown as it is. */
static void format_date (char *out, size_t size, const char *text) {
    int year = 0, month = 0, day = 0;
    int found = sscanf(text, "%4d-%2d-%2d", &year, &month, &day);
    if (found >= 2 && year >= 1000 && month >= 1 && month <= 12) {
        if (found == 3 && day >= 1 && day <= 31) {
            snprintf(out, size, "%s %d, %d", month_names[month - 1], day, year);
        } else {
            snprintf(out, size, "%s %d", month_names[month - 1], year);
        }
    } else {
        snprintf(out, size, "%s", text);
    }
}

/* The description, laid out once per game instead of every frame. If it
   is longer than its box, it is cut at a word and ends in "..." instead of
   stopping in the middle of a sentence. */
static rdpq_paragraph_t *desc_layout = NULL;
static uint32_t desc_key = 0;

static rdpq_paragraph_t *desc_build (const char *text, int length, int height) {
    int nbytes = length;
    rdpq_paragraph_t *layout = rdpq_paragraph_build(&(rdpq_textparms_t) {
        .width = X1 - X0 - (PAD * 2),
        .height = height,
        .wrap = WRAP_WORD,
    }, FNT_DEFAULT, text, &nbytes);

    for (int attempt = 0; attempt < 4 && nbytes < (length - 2); attempt++) {
        /* Not all of it fitted. Keep what did, less its last word or two to
           make room, and end it with "...". */
        static char cut[1024];
        int keep = (nbytes < (int) sizeof(cut) - 4) ? nbytes : ((int) sizeof(cut) - 4);
        for (int words = 0; words < (2 + attempt) && keep > 0; words++) {
            while (keep > 0 && text[keep - 1] == ' ') keep--;
            while (keep > 0 && text[keep - 1] != ' ') keep--;
        }
        while (keep > 0 && (text[keep - 1] == ' ' || text[keep - 1] == ',' || text[keep - 1] == '.' || text[keep - 1] == ';' || text[keep - 1] == ':')) keep--;
        if (keep <= 0) {
            break;
        }
        memcpy(cut, text, keep);
        memcpy(cut + keep, "...", 4);

        rdpq_paragraph_free(layout);
        length = keep + 3;
        nbytes = length;
        layout = rdpq_paragraph_build(&(rdpq_textparms_t) {
            .width = X1 - X0 - (PAD * 2),
            .height = height,
            .wrap = WRAP_WORD,
        }, FNT_DEFAULT, cut, &nbytes);
        text = cut;
    }
    return layout;
}

/* Scroll Descriptions: the whole text, laid out once, drawn through a window
   the size of the box. Left alone for SCROLL_WAIT_MS it drifts up, rests at
   the end, then starts again from the top; the D-Pad or stick moves it by
   hand at any time. Timed by the clock. */
#define SCROLL_WAIT_MS      (10000)
#define SCROLL_END_MS       (4000)
#define SCROLL_SPEED        (14.0f)     /* pixels a second on its own */
#define SCROLL_HAND_SPEED   (120.0f)    /* pixels a second, D-Pad or stick held all the way */
#define SCROLL_DEADZONE     (12)

static rdpq_paragraph_t *scroll_layout = NULL;
static uint32_t scroll_key = 0;
static float scroll_offset = 0.0f;
static uint64_t scroll_still_since = 0;     /* last time it was at the top or moved by hand */
static uint64_t scroll_end_since = 0;       /* reached the end on its own; 0 = not there */
static uint64_t scroll_last_ms = 0;

/* -1 to 1: up is toward the start of the text. */
static float scroll_input (void) {
    float most = 0.0f;
    JOYPAD_PORT_FOREACH (port) {
        joypad_inputs_t in = joypad_get_inputs(port);
        float v = 0.0f;
        if (in.btn.d_up) {
            v = 1.0f;
        } else if (in.btn.d_down) {
            v = -1.0f;
        } else if (in.stick_y > SCROLL_DEADZONE || in.stick_y < -SCROLL_DEADZONE) {
            v = in.stick_y / 80.0f;
            v = (v > 1.0f) ? 1.0f : ((v < -1.0f) ? -1.0f : v);
        }
        if (fabsf(v) > fabsf(most)) {
            most = v;
        }
    }
    return most;
}

/* True if it drew the text (it was longer than the box). */
static bool draw_description_scrolling (const char *text, int height) {
    uint64_t now = get_ticks_ms();
    uint32_t key = text_hash(text) ^ (uint32_t) height ^ ((uint32_t) (uintptr_t) theme_get()->text.r << 24);
    if (!scroll_layout || key != scroll_key) {
        if (scroll_layout) {
            rdpq_paragraph_free(scroll_layout);
        }
        int nbytes = strlen(text);
        scroll_layout = rdpq_paragraph_build(&(rdpq_textparms_t) {
            .width = X1 - X0 - (PAD * 2),
            .height = 4000,         /* no real limit; a height puts the origin at the box's top, as below */
            .wrap = WRAP_WORD,
        }, FNT_DEFAULT, text, &nbytes);
        scroll_key = key;
        scroll_offset = 0.0f;
        scroll_still_since = now;
        scroll_end_since = 0;
    }
    if (now - scroll_last_ms > 500) {
        /* The screen has just been opened (again): start from the top. */
        scroll_offset = 0.0f;
        scroll_still_since = now;
        scroll_end_since = 0;
    }
    float seconds = (now - scroll_last_ms) / 1000.0f;
    scroll_last_ms = now;
    if (seconds > 0.1f) {
        seconds = 0.1f;
    }

    float overflow = scroll_layout->bbox.y1 - height + 4;
    if (overflow <= 0.0f) {
        return false;
    }

    float hand = scroll_input();
    if (hand != 0.0f) {
        scroll_offset -= hand * SCROLL_HAND_SPEED * seconds;
        scroll_still_since = now;
        scroll_end_since = 0;
    } else if (now - scroll_still_since > SCROLL_WAIT_MS) {
        if (scroll_offset < overflow) {
            scroll_offset += SCROLL_SPEED * seconds;
        } else if (scroll_end_since == 0) {
            scroll_end_since = now;
        } else if (now - scroll_end_since > SCROLL_END_MS) {
            scroll_offset = 0.0f;       /* back to the start, and wait again */
            scroll_still_since = now;
            scroll_end_since = 0;
        }
    }
    scroll_offset = (scroll_offset < 0.0f) ? 0.0f : ((scroll_offset > overflow) ? overflow : scroll_offset);

    int top = DESC_Y + 20;
    rdpq_set_scissor(X0 + 2, top - 4, X1 - 2, top + height);
    /* Moved in whole steps of 2: with 240p output two of these lines are one
       line on the TV, and a step of 1 made the letters shimmer as they went. */
    rdpq_paragraph_render(scroll_layout, X0 + PAD, top - ((int) scroll_offset & ~1));
    rdpq_set_scissor(0, 0, display_get_width(), display_get_height());
    return true;
}

static void draw_description (const char *text, int height) {
    if (features_enabled(FEATURE_DESC_SCROLL) && draw_description_scrolling(text, height)) {
        return;
    }
    uint32_t key = text_hash(text) ^ (uint32_t) height ^ ((uint32_t) (uintptr_t) theme_get()->text.r << 24);
    if (!desc_layout || key != desc_key) {
        if (desc_layout) {
            rdpq_paragraph_free(desc_layout);
        }
        desc_layout = desc_build(text, (int) strlen(text), height);
        desc_key = key;
    }
    rdpq_paragraph_render(desc_layout, X0 + PAD, DESC_Y + 20);
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

    /* The genre, when it came out and who made it, above the title: the
       developer last, so a long name is what gets cut short, not the date. */
    const game_data_t *data = game_data_find(info->game_code);
    const char *genre = (data && data->genre[0]) ? data->genre : NULL;
    if (genre || known(info->meta.author) || known(info->meta.release_date)) {
        char released[32] = "";
        if (known(info->meta.release_date)) {
            format_date(released, sizeof(released), info->meta.release_date);
        }
        rdpq_text_printf(&(rdpq_textparms_t) {
            .width = TITLE_WIDTH,
            .wrap = WRAP_ELLIPSES,
            .style_id = STL_GRAY,
        }, FNT_DEFAULT, X0, MAKER_Y + 14, "%s%s%s%s%s",
            genre ? genre : "",
            (genre && (released[0] || known(info->meta.author))) ? " \xC2\xB7 " : "",
            released,
            (released[0] && known(info->meta.author)) ? " \xC2\xB7 " : "",
            known(info->meta.author) ? info->meta.author : "");
    }

    /* The name. "Series - Subtitle" is shown as the series in big letters
       with the subtitle smaller underneath; any other name as big as fits
       in two lines. */
    const char *dash = strstr(view->name, " - ");
    char series[96];
    if (dash && dash > view->name && dash[3] != '\0' && (size_t) (dash - view->name) < sizeof(series)) {
        memcpy(series, view->name, dash - view->name);
        series[dash - view->name] = '\0';
        const char *subtitle = dash + 3;

        rdpq_text_printf(&(rdpq_textparms_t) {
            .width = TITLE_WIDTH,
            .height = TITLE_MAIN_HEIGHT,
            .wrap = WRAP_ELLIPSES,
        }, title_font_pick(series, TITLE_WIDTH - 16), X0, TITLE_Y, "%s", series);

        int font = title_font_pick(subtitle, TITLE_WIDTH - 16);
        if (font == FNT_TITLE) {
            font = FNT_TITLE_MEDIUM; /* never as big as the series */
        }
        rdpq_text_printf(&(rdpq_textparms_t) {
            .width = TITLE_WIDTH,
            .height = SUBTITLE_HEIGHT,
            .wrap = (font == FNT_DEFAULT) ? WRAP_WORD : WRAP_ELLIPSES,
            .style_id = STL_GRAY,
        }, font, X0, SUBTITLE_Y, "%s", subtitle);
    } else {
        rdpq_text_printf(&(rdpq_textparms_t) {
            .width = TITLE_WIDTH,
            .height = TITLE_HEIGHT,
            .wrap = WRAP_WORD,
        }, title_font_pick(view->name, (TITLE_WIDTH * 2) - 32), X0, TITLE_Y, "%s", view->name);
    }

    /* Whether this copy is a known good dump (Card Prep's verified.txt). */
    const char *rom_path = menu->load.rom_path ? path_get(menu->load.rom_path) : NULL;
    dump_state_t dump = dump_check(rom_path, -1, rom_path && games_filter_is_hack_path(menu, rom_path));

    /* What the game supports. */
    int x = X0 - 6;
    if (info->features.expansion_pak == EXPANSION_PAK_REQUIRED) {
        /* A warning only when the console has none. */
        x = is_memory_expanded()
            ? games_ui_badge(x, BADGES_Y, "Uses Expansion Pak", STL_GREEN, true)
            : games_ui_badge(x, BADGES_Y, "Needs Expansion Pak", STL_ORANGE, true);
    } else if (info->features.expansion_pak == EXPANSION_PAK_RECOMMENDED) {
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

    /* Played, last played, players. (The save type, TV region and release
       date boxes made way: the date moved up beside the maker, the other two
       can still be seen and changed under Options.) */
    time_t last = 0;
    int plays = menu->load.rom_path ? play_stats_get(path_get(menu->load.rom_path), &last) : 0;

    char played[24];
    if (plays <= 0) {
        snprintf(played, sizeof(played), "Never");
    } else {
        snprintf(played, sizeof(played), (plays == 1) ? "1 time" : "%d times", plays);
    }
    char last_played[24];
    format_last_played(last_played, sizeof(last_played), last, menu->current_time, plays);

    /* Players: the count and how they play together, from the built-in
       table (game_data.c), else the game's metadata. */
    char players[40];
    const char *together = game_data_players_text(data, false);
    if (together[0]) {
        snprintf(players, sizeof(players), "%s", together);
    } else if (info->meta.num_players > 1) {
        snprintf(players, sizeof(players), "1-%d", (int) info->meta.num_players);
    } else {
        snprintf(players, sizeof(players), "1");
    }

    draw_fact(0, "Played", played);
    draw_fact(1, "Last played", last_played);
    draw_fact(2, "Players", players);

    /* Description. This game's own switches are only mentioned when one
       of them is on, as badges along the bottom of the box. */
    bool switches = view->cheats || view->patches || view->clear_rdram;
    fill(X0, DESC_Y, X1, DESC_Y + DESC_HEIGHT, panel_color());
    bool note = (dump == DUMP_UNVERIFIED || dump == DUMP_BAD);
    draw_description(view->description, DESC_HEIGHT - 30 - (switches ? DESC_SWITCHES : 0) - (note ? DESC_NOTE : 0));
    if (note) {
        /* A quiet word, not a warning in the way: the mark says the rest. */
        rdpq_text_printf(&(rdpq_textparms_t) {
            .width = X1 - X0 - (PAD * 2),
            .height = DESC_NOTE,
            .wrap = WRAP_WORD,
            .style_id = STL_GRAY,
        }, FNT_DEFAULT, X0 + PAD, DESC_Y + DESC_HEIGHT - (switches ? DESC_SWITCHES : 0) - DESC_NOTE + 4, "%s",
            (dump == DUMP_BAD)    /* each fits two lines; a third would be cut off */
                ? "A bad dump: it may freeze or glitch. Find a copy that Card Prep or CRC32/SHA-1 verifies."
                : "Not a known good dump. If it misbehaves, find a copy that Card Prep or CRC32/SHA-1 verifies.");
    }
    if (switches) {
        int bx = X0 + PAD - 6;
        int by = DESC_Y + DESC_HEIGHT - DESC_SWITCHES - 2;
        if (view->cheats) bx = games_ui_badge(bx, by, "Cheats on", STL_ORANGE, true);
        if (view->patches) bx = games_ui_badge(bx, by, "Patches on", STL_ORANGE, true);
        if (view->clear_rdram) bx = games_ui_badge(bx, by, "Clears memory", STL_ORANGE, true);
    }

    /* Button hints, in the same two rows as the Games screen, on the same
       dark band so they can be read over bright art. */
    games_ui_hints_backdrop_draw();
    x = games_ui_hint_draw(GAMES_UI_HINTS_X, 0, "A", "Play");
    games_ui_hint_draw(x, 0, "B", "Back");
    games_ui_hint_draw(GAMES_UI_HINTS_X, 1, "R", "Options");
    games_ui_hint_right_draw(0, "START", "Details");
    games_ui_hint_right_draw(1, "◀▶", "Pictures");
    if (dump != DUMP_NONE) {
        /* Between the hints, out of the way of the badges and the cover. */
        games_ui_hint_mark_draw(1, dump, dump_check_label(dump),
            (dump == DUMP_VERIFIED) ? STL_GREEN : (dump == DUMP_BAD) ? STL_RED : STL_ORANGE);
    }
}
