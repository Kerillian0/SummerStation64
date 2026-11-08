/**
 * @file logo_shots.c
 * @brief Test build only (FLAGS=-DLOGO_SHOTS, see tests/logo_shots): opens
 *        Game info for every game named in rom:/shots/list.txt, one after
 *        another, and sends each finished screen out through the emulator's
 *        debug log (ISViewer) as base64 text, for tests/logo_shots/shots.py
 *        to turn back into pictures. Without the flag this file is empty.
 *
 * Each game is opened the way Recent opens one (its path in the first
 * history slot), so Game info runs exactly as it does for a player. The
 * screen is sent once its cover and backdrop are in and a few more frames
 * have been drawn, or after a while if there is no cover.
 */

#include "logo_shots.h"

#ifdef LOGO_SHOTS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libdragon.h>

#include "bookkeeping.h"
#include "game_info_ui.h"
#include "path.h"

#define LIST            "rom:/shots/list.txt"
#define FOLDER          "shots"
#define START_FRAMES    (150)       /* the menu settles on its first screen first */
#define READY_FRAMES    (10)        /* drawn after the cover and backdrop are in, before the picture is taken */
#define GIVE_UP_FRAMES  (90)        /* taken anyway after this many: no cover, or an error */
#define CHUNK           (3 * 1024)  /* bytes per line of base64; a multiple of 3, so only a line's end is padded */

typedef enum {
    SHOTS_WAITING,      /* for the menu to settle */
    SHOTS_LEAVING,      /* closing the screen shown */
    SHOTS_OPENING,      /* opening the next game */
    SHOTS_SHOWING,      /* Game info is up: waiting until it is complete */
    SHOTS_DONE,
} shots_stage_t;

static shots_stage_t stage = SHOTS_WAITING;
static int frames = 0;
static int ready_frames = 0;
static char **names = NULL;
static int count = 0;
static int current = 0;
static path_t *opened = NULL;

static void read_list (void) {
    FILE *f = fopen(LIST, "r");
    if (!f) {
        fprintf(stderr, "\n@@SHOTS no list at " LIST "\n");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) {
            continue;
        }
        char **more = realloc(names, (count + 1) * sizeof(char *));
        if (!more) {
            break;
        }
        names = more;
        names[count++] = strdup(line);
    }
    fclose(f);
}

/* Each line decodes on its own, so a row that isn't a multiple of 3 bytes
   only pads its own last line. */
static void put_base64 (const uint8_t *data, int size) {
    static const char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static char line[(CHUNK / 3) * 4 + 2];
    for (int at = 0; at < size; at += CHUNK) {
        int n = (size - at < CHUNK) ? (size - at) : CHUNK;
        int out = 0;
        for (int i = 0; i < n; i += 3) {
            uint32_t v = (uint32_t) data[at + i] << 16;
            if (i + 1 < n) v |= (uint32_t) data[at + i + 1] << 8;
            if (i + 2 < n) v |= data[at + i + 2];
            line[out++] = digits[(v >> 18) & 63];
            line[out++] = digits[(v >> 12) & 63];
            line[out++] = (i + 1 < n) ? digits[(v >> 6) & 63] : '=';
            line[out++] = (i + 2 < n) ? digits[v & 63] : '=';
        }
        line[out++] = '\n';
        fwrite(line, 1, out, stderr);
    }
}

/* The frame as drawn: 16-bit RGBA5551, big-endian, row after row. */
static void send (surface_t *display) {
    int w = display->width;
    int h = display->height;
    const uint8_t *pixels = VirtualUncachedAddr(display->buffer);  /* what the graphics chip wrote */
    fprintf(stderr, "\n@@SHOT %d %d %d %s\n", current, w, h, names[current]);
    if (display->stride == w * 2) {
        put_base64(pixels, w * h * 2);
    } else {
        for (int y = 0; y < h; y++) {
            put_base64(pixels + y * display->stride, w * 2);
        }
    }
    fprintf(stderr, "@@END %d\n", current);
}

void logo_shots_after_frame (menu_t *menu, surface_t *display) {
    frames++;
    switch (stage) {
        case SHOTS_WAITING:
            if (frames < START_FRAMES) {
                return;
            }
            read_list();
            fprintf(stderr, "\n@@SHOTS %d\n", count);
            if (count == 0) {
                stage = SHOTS_DONE;
                return;
            }
            stage = SHOTS_LEAVING;
            /* fall through */

        case SHOTS_LEAVING:
            menu->next_mode = MENU_MODE_NONE;   /* the screen shown closes itself on its next frame */
            stage = SHOTS_OPENING;
            return;

        case SHOTS_OPENING: {
            if (menu->mode != MENU_MODE_NONE) {
                return;
            }
            path_t *previous = opened;
            opened = path_init("rom:/", FOLDER);
            path_push(opened, names[current]);
            menu->bookkeeping.history_items[0].primary_path = opened;
            menu->bookkeeping.history_items[0].bookkeeping_type = BOOKKEEPING_TYPE_ROM;
            menu->load.load_history_id = 0;
            menu->load.load_favorite_id = -1;
            menu->next_mode = MENU_MODE_LOAD_ROM;
            if (previous) {
                path_free(previous);
            }
            frames = 0;
            ready_frames = 0;
            stage = SHOTS_SHOWING;
            return;
        }

        case SHOTS_SHOWING:
            if (menu->mode == MENU_MODE_LOAD_ROM && !game_info_ui_backdrop_ready(menu) && frames < GIVE_UP_FRAMES) {
                return;
            }
            if (++ready_frames < READY_FRAMES) {
                return;
            }
            send(display);
            if (++current >= count) {
                fprintf(stderr, "\n@@DONE\n");
                stage = SHOTS_DONE;
                return;
            }
            menu->next_mode = MENU_MODE_NONE;
            stage = SHOTS_OPENING;
            return;

        case SHOTS_DONE:
            return;
    }
}

#endif /* LOGO_SHOTS */
