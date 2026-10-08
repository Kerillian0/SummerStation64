/**
 * @file folders_ui.c
 * @brief The plain file list on the Folders tab.
 */

#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "folders_ui.h"
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

void folders_ui_draw (menu_t *menu) {
    const theme_t *t = theme_get();
    int entries = menu->browser.entries;
    int selected = menu->browser.selected;
    int name_width = LIST_X1 - LIST_X0 - SIZE_WIDTH - BAR_WIDTH - (TEXT_INSET * 3);

    /* Where we are. The "sd:/" at the front tells the player nothing. */
    const char *where = path_get(menu->browser.directory);
    const char *slash = strchr(where, '/');
    text(LIST_X0 + TEXT_INSET, PATH_Y, LIST_X1 - LIST_X0 - TEXT_INSET, ALIGN_LEFT, STL_GRAY, (slash && slash[1]) ? slash : "/");

    if (entries <= 0) {
        text(LIST_X0, ROWS_Y + ROW_HEIGHT, LIST_X1 - LIST_X0, ALIGN_CENTER, STL_GRAY, "This folder is empty");
        return;
    }

    /* Keep the selected row near the middle while there is more to scroll to. */
    int first = 0;
    if (entries > ROWS) {
        first = selected - (ROWS / 2);
        if (first < 0) first = 0;
        if (first > entries - ROWS) first = entries - ROWS;
    }

    for (int row = 0; row < ROWS && (first + row) < entries; row++) {
        const entry_t *entry = &menu->browser.list[first + row];
        int y = ROWS_Y + (row * ROW_HEIGHT);

        if ((first + row) == selected) {
            fill(LIST_X0, y, LIST_X1 - BAR_WIDTH - 2, y + ROW_HEIGHT, t->highlight);
            fill(LIST_X0, y + ROW_HEIGHT - 2, LIST_X1 - BAR_WIDTH - 2, y + ROW_HEIGHT, t->accent);
        }

        bool folder = (entry->type == ENTRY_TYPE_DIR);
        text(LIST_X0 + TEXT_INSET, y, name_width, ALIGN_LEFT, folder ? STL_YELLOW : STL_DEFAULT, entry->name);
        text(LIST_X1 - BAR_WIDTH - TEXT_INSET - SIZE_WIDTH, y, SIZE_WIDTH - TEXT_INSET, ALIGN_RIGHT, STL_GRAY, size_text(entry));
    }

    if (entries > ROWS) {
        int track = ROWS * ROW_HEIGHT;
        int thumb = (track * ROWS) / entries;
        if (thumb < 8) thumb = 8;
        int thumb_y = ROWS_Y + ((track - thumb) * first) / (entries - ROWS);
        fill(LIST_X1 - BAR_WIDTH, ROWS_Y, LIST_X1, ROWS_Y + track, t->tab_inactive);
        fill(LIST_X1 - BAR_WIDTH, thumb_y, LIST_X1, thumb_y + thumb, t->accent);
    }
}
