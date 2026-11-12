#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_status_list.h"

static const char letters[GAME_STATUS_COUNT] = { 0, 'P', 'B', 'C', 'L' };

static uint32_t name_hash (const char *name) {
    uint32_t h = 2166136261u;
    for (const unsigned char *c = (const unsigned char *) name; *c; c++) {
        h = (h ^ *c) * 16777619u;
    }
    return h;
}

bool game_status_parse (const char *line, game_status_t *status, char *name, size_t name_size) {
    if (!line[0] || line[1] != ' ') {
        return false;
    }
    game_status_t found = GAME_STATUS_NONE;
    for (int s = GAME_STATUS_PLAYING; s < GAME_STATUS_COUNT; s++) {
        if (line[0] == letters[s]) {
            found = (game_status_t) s;
        }
    }
    if (found == GAME_STATUS_NONE) {
        return false;
    }
    size_t length = strcspn(&line[2], "\r\n");
    if (length == 0 || length >= name_size) {
        return false;
    }
    memcpy(name, &line[2], length);
    name[length] = '\0';
    *status = found;
    return true;
}

int game_status_format (game_status_t status, const char *name, char *out, size_t size) {
    if (status <= GAME_STATUS_NONE || status >= GAME_STATUS_COUNT) {
        return snprintf(out, size, "%s", "");
    }
    return snprintf(out, size, "%c %s\n", letters[status], name);
}

static int find (const game_status_list_t *list, const char *name) {
    uint32_t h = name_hash(name);
    for (int i = 0; i < list->count; i++) {
        if (list->rows[i].hash == h && strcmp(list->rows[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

game_status_t game_status_list_get (const game_status_list_t *list, const char *name) {
    int i = find(list, name);
    return (i < 0) ? GAME_STATUS_NONE : (game_status_t) list->rows[i].status;
}

static void remove_row (game_status_list_t *list, int i) {
    free(list->rows[i].name);
    list->rows[i] = list->rows[--list->count];
}

bool game_status_list_set (game_status_list_t *list, const char *name, game_status_t status) {
    int i = find(list, name);
    if (status <= GAME_STATUS_NONE || status >= GAME_STATUS_COUNT) {
        if (i >= 0) {
            remove_row(list, i);
        }
        return true;
    }
    if (i >= 0) {
        list->rows[i].status = (uint8_t) status;
        return true;
    }
    if (list->count >= GAME_STATUS_MAX) {
        return false;
    }
    char *copy = strdup(name);
    if (!copy) {
        return false;
    }
    game_status_row_t *row = &list->rows[list->count++];
    row->hash = name_hash(name);
    row->status = (uint8_t) status;
    row->name = copy;
    return true;
}

bool game_status_list_rename (game_status_list_t *list, const char *old_name, const char *new_name) {
    int i = find(list, old_name);
    if (i < 0 || find(list, new_name) >= 0) {
        return false;
    }
    char *copy = strdup(new_name);
    if (!copy) {
        return false;
    }
    free(list->rows[i].name);
    list->rows[i].name = copy;
    list->rows[i].hash = name_hash(new_name);
    return true;
}

int game_status_list_count (const game_status_list_t *list, game_status_t status) {
    int n = 0;
    for (int i = 0; i < list->count; i++) {
        n += (list->rows[i].status == status) ? 1 : 0;
    }
    return n;
}

void game_status_list_clear (game_status_list_t *list) {
    for (int i = 0; i < list->count; i++) {
        free(list->rows[i].name);
    }
    list->count = 0;
}
