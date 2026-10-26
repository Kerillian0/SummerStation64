/**
 * @file cheat_menu.c
 * @brief Z on a game's Game info: its cheats, to switch on and off.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

#include "cheat_menu.h"
#include "cheat_ready.h"
#include "fonts.h"
#include "menu_features.h"
#include "path.h"
#include "rom_info.h"
#include "safe_file.h"
#include "sound.h"
#include "ui_components.h"
#include "ui_components/constants.h"
#include "utils/fs.h"
#include "views/views.h"

#define FILE_MAX        (64 * 1024)     /* a cheat file larger than this is not opened */
#define CHEATS_MAX      (128)
#define NAME_LENGTH     (48)
#define ROWS_SHOWN      (9)
#define ROW_HEIGHT      (26)
#define LIST_WIDTH      (460)

static bool showing = false;
static bool changed = false;
static char file_path[256];
static char *text = NULL;           /* the file as read, one line each, '\0' at each line end */
static char **lines = NULL;         /* each line's start */
static int line_count = 0;
static struct {
    int first;                      /* its first line */
    int count;                      /* its lines (the named one and those under it) */
    bool on;
    char name[NAME_LENGTH];
} cheats[CHEATS_MAX];
static int cheat_count = 0;
static int row = 0, first_row = 0;

/* A file's path with another ending: "sd:/N64/Game.z64" -> "sd:/N64/Game.datel". */
static bool path_with (const char *rom_path, const char *ending, char *out, size_t size) {
    path_t *p = path_create(rom_path);
    path_ext_replace(p, (char *) ending);
    snprintf(out, size, "%s", path_get(p));
    path_free(p);
    return file_exists(out);
}

bool cheat_menu_available (const char *rom_path) {
    char p[256];
    return path_with(rom_path, "datel", p, sizeof(p)) || path_with(rom_path, "datel.txt", p, sizeof(p));
}

/* A code line: 8 hex digits, a space (or colon), 4 hex digits; `name` gets what follows. */
static bool code_line (const char *s, const char **name) {
    for (int i = 0; i < 8; i++) {
        if (!isxdigit((unsigned char) s[i])) return false;
    }
    s += 8;
    while (*s == ' ' || *s == ':' || *s == '-' || *s == '\t') s++;
    for (int i = 0; i < 4; i++) {
        if (!isxdigit((unsigned char) s[i])) return false;
    }
    s += 4;
    while (*s == ' ' || *s == '\t') s++;
    *name = s;
    return true;
}

/* A line's code part, after any ": " that switches it off. */
static const char *code_part (const char *line, bool *off) {
    while (*line == ' ' || *line == '\t') line++;
    *off = (*line == ':');
    if (*off) {
        line++;
        while (*line == ' ' || *line == '\t') line++;
    }
    return line;
}

static void forget (void) {
    free(text);
    free(lines);
    text = NULL;
    lines = NULL;
    line_count = 0;
    cheat_count = 0;
}

static bool read_file (void) {
    FILE *f = fopen(file_path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > FILE_MAX) {
        fclose(f);
        return false;
    }
    text = malloc(size + 1);
    lines = malloc(sizeof(char *) * (size / 2 + 2));
    bool ok = text && lines && fread(text, 1, size, f) == (size_t) size;
    fclose(f);
    if (!ok) {
        forget();
        return false;
    }
    text[size] = '\0';
    for (char *s = text; *s; ) {
        lines[line_count++] = s;
        char *end = s + strcspn(s, "\r\n");
        if (*end == '\r' && end[1] == '\n') { *end = '\0'; s = end + 2; }
        else if (*end) { *end = '\0'; s = end + 1; }
        else s = end;
    }
    /* Cheats: a code line with a name starts one; nameless code lines join it. */
    for (int i = 0; i < line_count; i++) {
        bool off;
        const char *name;
        if (!code_line(code_part(lines[i], &off), &name)) {
            continue;
        }
        if (name[0] || cheat_count == 0 || cheats[cheat_count - 1].first + cheats[cheat_count - 1].count != i) {
            if (cheat_count >= CHEATS_MAX) break;
            cheats[cheat_count].first = i;
            cheats[cheat_count].count = 1;
            cheats[cheat_count].on = !off;
            snprintf(cheats[cheat_count].name, NAME_LENGTH, "%s", name[0] ? name : "Cheat");
            cheat_count++;
        } else {
            cheats[cheat_count - 1].count++;
        }
    }
    return true;
}

static bool write_file (void) {
    char temp[260];
    snprintf(temp, sizeof(temp), "%s.tmp", file_path);
    FILE *f = fopen(temp, "w");
    if (!f) return false;
    int c = 0;
    for (int i = 0; i < line_count; i++) {
        while (c < cheat_count && i >= cheats[c].first + cheats[c].count) c++;
        bool in = (c < cheat_count && i >= cheats[c].first);
        if (in) {
            bool off;
            const char *code = code_part(lines[i], &off);
            fprintf(f, "%s%s\n", cheats[c].on ? "" : ": ", code);
        } else {
            fprintf(f, "%s\n", lines[i]);
        }
    }
    bool ok = (fclose(f) == 0);
    return ok && safe_file_replace(temp, file_path);
}

static int on_count (void) {
    int n = 0;
    for (int i = 0; i < cheat_count; i++) n += cheats[i].on ? 1 : 0;
    return n;
}

void cheat_menu_open (menu_t *menu) {
    forget();
    const char *rom = path_get(menu->load.rom_path);
    if (!path_with(rom, "datel", file_path, sizeof(file_path)) && !path_with(rom, "datel.txt", file_path, sizeof(file_path))) {
        return;
    }
    if (!read_file()) {
        menu_show_error(menu, "Couldn't read this game's cheat file");
        return;
    }
    if (cheat_count == 0) {
        forget();
        menu_show_error(menu, "No cheats in this game's cheat file");
        return;
    }
    showing = true;
    changed = false;
    row = 0;
    first_row = 0;
}

static void close_list (menu_t *menu) {
    showing = false;
    if (changed) {
        if (write_file()) {
            debugf("cheats: saved %s, %d of %d on\n", file_path, on_count(), cheat_count);
        } else {
            debugf("cheats: could not save %s\n", file_path);
        }
        cheat_ready_forget();   /* read again when the game starts */
        /* Any cheat on: Use Cheats on (it needs the Expansion Pak); none: off. */
        bool want = on_count() > 0 && is_memory_expanded();
        if (menu->load.rom_info.settings.cheats_enabled != want) {
            rom_config_setting_set_cheats(menu->load.rom_path, &menu->load.rom_info, want);
            menu->browser.reload = true;
        }
    }
    forget();
}

bool cheat_menu_process (menu_t *menu) {
    if (!showing) {
        return false;
    }
    int step = menu->actions.go_fast ? ROWS_SHOWN : 1;
    if (menu->actions.go_up && row > 0) {
        row = (row - step < 0) ? 0 : row - step;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down && row < cheat_count - 1) {
        row = (row + step >= cheat_count) ? cheat_count - 1 : row + step;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.enter) {
        cheats[row].on = !cheats[row].on;
        changed = true;
        sound_play_effect(SFX_SETTING);
    } else if (menu->actions.back || menu->actions.lz_context) {
        close_list(menu);
        sound_play_effect(SFX_EXIT);
        return true;
    }
    if (row < first_row) {
        first_row = row;
    } else if (row >= first_row + ROWS_SHOWN) {
        first_row = row - ROWS_SHOWN + 1;
    }
    return true;
}

void cheat_menu_draw (void) {
    if (!showing) {
        return;
    }
    int shown = (cheat_count < ROWS_SHOWN) ? cheat_count : ROWS_SHOWN;
    int height = 40 + (shown * ROW_HEIGHT) + 34;
    ui_components_dialog_draw(LIST_WIDTH, height);
    int x0 = DISPLAY_CENTER_X - (LIST_WIDTH / 2);
    int y0 = DISPLAY_CENTER_Y - (height / 2);
    rdpq_text_printf(&(rdpq_textparms_t) { .width = LIST_WIDTH, .height = 26, .align = ALIGN_CENTER, .valign = VALIGN_CENTER,
        .style_id = STL_GRAY }, FNT_DEFAULT, x0, y0 + 6, is_memory_expanded() ? "Cheats: %d of %d on" : "Cheats need the Expansion Pak",
        on_count(), cheat_count);
    for (int i = 0; i < shown; i++) {
        int r = first_row + i;
        int y = y0 + 34 + (i * ROW_HEIGHT);
        if (r == row) {
            ui_components_box_draw(x0 + 8, y, x0 + LIST_WIDTH - 8, y + ROW_HEIGHT, CONTEXT_MENU_HIGHLIGHT_COLOR);
        }
        rdpq_text_printf(&(rdpq_textparms_t) { .width = 24, .height = ROW_HEIGHT, .valign = VALIGN_CENTER, .style_id = STL_GREEN },
            FNT_DEFAULT, x0 + 20, y, "%s", cheats[r].on ? "✓" : "");
        rdpq_text_printf(&(rdpq_textparms_t) { .width = LIST_WIDTH - 96, .height = ROW_HEIGHT, .valign = VALIGN_CENTER,
            .wrap = WRAP_ELLIPSES, .style_id = cheats[r].on ? STL_DEFAULT : STL_GRAY }, FNT_DEFAULT, x0 + 48, y, "%s", cheats[r].name);
    }
    if (first_row > 0) {
        rdpq_text_printf(&(rdpq_textparms_t) { .width = 20, .height = ROW_HEIGHT, .valign = VALIGN_CENTER }, FNT_DEFAULT,
            x0 + LIST_WIDTH - 32, y0 + 34, "▲");
    }
    if (first_row + shown < cheat_count) {
        rdpq_text_printf(&(rdpq_textparms_t) { .width = 20, .height = ROW_HEIGHT, .valign = VALIGN_CENTER }, FNT_DEFAULT,
            x0 + LIST_WIDTH - 32, y0 + 34 + ((shown - 1) * ROW_HEIGHT), "▼");
    }
    rdpq_text_printf(&(rdpq_textparms_t) { .width = LIST_WIDTH, .height = 26, .align = ALIGN_CENTER, .valign = VALIGN_CENTER,
        .style_id = STL_GRAY }, FNT_DEFAULT, x0, y0 + 40 + (shown * ROW_HEIGHT), "A: On / off    B: Done");
}
