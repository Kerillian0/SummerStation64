/**
 * @file folders_ui.c
 * @brief The plain file list on the Folders tab.
 */

#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "folders_ui.h"
#include "tabs.h"
#include "fonts.h"
#include "games_ui.h"
#include "theme.h"
#include "ui_components/constants.h"

#define LIST_X0         (VISIBLE_AREA_X0 + 16)
#define LIST_X1         (VISIBLE_AREA_X1 - 16)
#define PATH_Y          (GAMES_UI_LIST_TOP)
#define ROW_HEIGHT      (20)
#define ROWS_Y          (PATH_Y + ROW_HEIGHT + 2)
#define ROWS            (15)
#define SIZE_WIDTH      (84)    /* room for "1023 MB" or "Folder" */
#define BAR_WIDTH       (4)
#define TEXT_INSET      (6)
#define TAB_SLIDE_PX    (200)   /* how far the list travels when the tab is switched to; the covers' distance */

static void fill (int x0, int y0, int x1, int y1, color_t color) {
    rdpq_mode_push();
        rdpq_set_mode_fill(color);
        rdpq_fill_rectangle(x0, y0, x1, y1);
    rdpq_mode_pop();
}

static void text (int x, int y, int width, rdpq_align_t align, menu_font_style_t style, const char *string) {
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = width,
        .height = ROW_HEIGHT,
        .align = align,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES,
        .style_id = style,
    }, FNT_DEFAULT, x, y, "%s", string);
}

static const char *size_text (const entry_t *entry) {
    static char buffer[16];

    if (entry->type == ENTRY_TYPE_DIR) {
        return "Folder";
    }
    if (entry->size < 0) {
        return "";
    }
    if (entry->size < 1024) {
        snprintf(buffer, sizeof(buffer), "%d B", (int) entry->size);
    } else if (entry->size < (1024 * 1024)) {
        snprintf(buffer, sizeof(buffer), "%d KB", (int) (entry->size / 1024));
    } else {
        snprintf(buffer, sizeof(buffer), "%d MB", (int) (entry->size / (1024 * 1024)));
    }
    return buffer;
}

/* Laying text out is the costly part of drawing it, and this screen has up
   to 31 pieces of it. So each row's name and size are laid out once and
   kept for as long as that file stays on screen; moving the selection or
   scrolling one step lays out one new row at most. */
typedef struct {
    uint32_t key;               /* which file this is; 0 = free */
    bool used;                  /* drawn this frame */
    rdpq_paragraph_t *name;
    rdpq_paragraph_t *size;
} row_text_t;

static row_text_t rows_kept[ROWS];
static rdpq_paragraph_t *path_kept = NULL;
static uint32_t path_key = 0;

static uint32_t text_hash (uint32_t hash, const char *string) {
    while (*string) {
        hash = (hash ^ (uint8_t) *string++) * 16777619u;
    }
    return hash;
}

static rdpq_paragraph_t *layout (int width, rdpq_align_t align, menu_font_style_t style, const char *string) {
    int nbytes = strlen(string);
    return rdpq_paragraph_build(&(rdpq_textparms_t) {
        .width = width,
        .height = ROW_HEIGHT,
        .align = align,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES,
        .style_id = style,
    }, FNT_DEFAULT, string, &nbytes);
}

static void row_free (row_text_t *row) {
    if (row->name) rdpq_paragraph_free(row->name);
    if (row->size) rdpq_paragraph_free(row->size);
    memset(row, 0, sizeof(*row));
}

void folders_ui_forget (void) {
    for (int i = 0; i < ROWS; i++) {
        row_free(&rows_kept[i]);
    }
    if (path_kept) {
        rdpq_paragraph_free(path_kept);
        path_kept = NULL;
    }
    path_key = 0;
}

static uint32_t row_key (const entry_t *entry) {
    uint32_t key = text_hash(2166136261u ^ (uint32_t) entry->type ^ ((uint32_t) entry->size * 31u), entry->name);
    return key ? key : 1;
}

/* The kept text for a file, laid out now if it is new on screen. Rows
   still on screen were marked `used` beforehand, so only the place of a
   file that has scrolled away is taken. */
static row_text_t *row_text (const entry_t *entry, int name_width) {
    uint32_t key = row_key(entry);
    row_text_t *spare = NULL;
    for (int i = 0; i < ROWS; i++) {
        if (rows_kept[i].key == key) {
            return &rows_kept[i];
        }
        if (!rows_kept[i].used && (!spare || rows_kept[i].key == 0)) {
            spare = &rows_kept[i];
        }
    }
    if (!spare) {
        return NULL;
    }
    row_free(spare);
    bool folder = (entry->type == ENTRY_TYPE_DIR);
    spare->key = key;
    spare->used = true;
    spare->name = layout(name_width, ALIGN_LEFT, folder ? STL_YELLOW : STL_DEFAULT, entry->name);
    spare->size = layout(SIZE_WIDTH - TEXT_INSET, ALIGN_RIGHT, STL_GRAY, size_text(entry));
    return spare;
}

void folders_ui_draw (menu_t *menu) {
    const theme_t *t = theme_get();
    int entries = menu->browser.entries;
    int selected = menu->browser.selected;
    int name_width = LIST_X1 - LIST_X0 - SIZE_WIDTH - BAR_WIDTH - (TEXT_INSET * 3);

    /* Just switched to this tab: the whole list comes in from that side. */
    int x0 = LIST_X0 + (int) (tabs_slide_folders() * TAB_SLIDE_PX);
    int x1 = x0 + (LIST_X1 - LIST_X0);

    /* Where we are. The "sd:/" at the front tells the player nothing. */
    const char *where = path_get(menu->browser.directory);
    const char *slash = strchr(where, '/');
    const char *shown = (slash && slash[1]) ? slash : "/";
    uint32_t key = text_hash(2166136261u, shown);
    if (!path_kept || key != path_key) {
        if (path_kept) {
            rdpq_paragraph_free(path_kept);
        }
        path_kept = layout(LIST_X1 - LIST_X0 - TEXT_INSET, ALIGN_LEFT, STL_GRAY, shown);
        path_key = key;
    }
    rdpq_paragraph_render(path_kept, x0 + TEXT_INSET, PATH_Y);

    if (entries <= 0) {
        text(x0, ROWS_Y + ROW_HEIGHT, LIST_X1 - LIST_X0, ALIGN_CENTER, STL_GRAY, "This folder is empty");
        return;
    }

    /* Keep the selected row near the middle while there is more to scroll to. */
    int first = 0;
    if (entries > ROWS) {
        first = selected - (ROWS / 2);
        if (first < 0) first = 0;
        if (first > entries - ROWS) first = entries - ROWS;
    }

    /* Mark the kept rows that are still on screen, so their places are not
       given away to new ones. */
    for (int i = 0; i < ROWS; i++) {
        rows_kept[i].used = false;
    }
    for (int row = 0; row < ROWS && (first + row) < entries; row++) {
        uint32_t key = row_key(&menu->browser.list[first + row]);
        for (int i = 0; i < ROWS; i++) {
            if (rows_kept[i].key == key) {
                rows_kept[i].used = true;
                break;
            }
        }
    }

    for (int row = 0; row < ROWS && (first + row) < entries; row++) {
        const entry_t *entry = &menu->browser.list[first + row];
        int y = ROWS_Y + (row * ROW_HEIGHT);

        if ((first + row) == selected) {
            fill(x0, y, x1 - BAR_WIDTH - 2, y + ROW_HEIGHT, t->highlight);
            fill(x0, y + ROW_HEIGHT - 2, x1 - BAR_WIDTH - 2, y + ROW_HEIGHT, t->accent);
        }

        row_text_t *kept = row_text(entry, name_width);
        if (kept) {
            rdpq_paragraph_render(kept->name, x0 + TEXT_INSET, y);
            rdpq_paragraph_render(kept->size, x1 - BAR_WIDTH - TEXT_INSET - SIZE_WIDTH, y);
        } else {
            /* Two files that look alike to the key: draw this one the plain way. */
            bool folder = (entry->type == ENTRY_TYPE_DIR);
            text(x0 + TEXT_INSET, y, name_width, ALIGN_LEFT, folder ? STL_YELLOW : STL_DEFAULT, entry->name);
            text(x1 - BAR_WIDTH - TEXT_INSET - SIZE_WIDTH, y, SIZE_WIDTH - TEXT_INSET, ALIGN_RIGHT, STL_GRAY, size_text(entry));
        }
    }

    if (entries > ROWS) {
        int track = ROWS * ROW_HEIGHT;
        int thumb = (track * ROWS) / entries;
        if (thumb < 8) thumb = 8;
        int thumb_y = ROWS_Y + ((track - thumb) * first) / (entries - ROWS);
        fill(x1 - BAR_WIDTH, ROWS_Y, x1, ROWS_Y + track, t->tab_inactive);
        fill(x1 - BAR_WIDTH, thumb_y, x1, thumb_y + thumb, t->accent);
    }
}
