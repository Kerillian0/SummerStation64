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

static bool beaten (int status) {
    return status == GAME_STATUS_BEATEN || status == GAME_STATUS_COMPLETE;
}

bool game_status_list_set (game_status_list_t *list, const char *name, game_status_t status) {
    return game_status_list_set_on(list, name, status, 0);
}

bool game_status_list_set_on (game_status_list_t *list, const char *name, game_status_t status, unsigned today) {
    int i = find(list, name);
    if (status <= GAME_STATUS_NONE || status >= GAME_STATUS_COUNT) {
        if (i >= 0) {
            remove_row(list, i);
        }
        return true;
    }
    if (i >= 0) {
        game_status_row_t *row = &list->rows[i];
        if (!beaten(status)) {
            row->day = 0;
        } else if (!beaten(row->status)) {
            row->day = (uint16_t) today;
        }
        row->status = (uint8_t) status;
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
    row->day = beaten(status) ? (uint16_t) today : 0;
    row->name = copy;
    return true;
}

bool game_status_parse_day (const char *line, unsigned *day, char *name, size_t name_size) {
    if (line[0] != 'D' || line[1] != ' ') {
        return false;
    }
    const char *p = &line[2];
    unsigned long n = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9' && digits < 6) {
        n = (n * 10) + (unsigned long) (*p++ - '0');
        digits++;
    }
    if (digits == 0 || *p != ' ' || n == 0 || n > 0xFFFF) {
        return false;
    }
    p++;
    size_t length = strcspn(p, "\r\n");
    if (length == 0 || length >= name_size) {
        return false;
    }
    memcpy(name, p, length);
    name[length] = '\0';
    *day = (unsigned) n;
    return true;
}

int game_status_format_day (const game_status_row_t *row, char *out, size_t size) {
    if (!beaten(row->status) || row->day == 0) {
        return snprintf(out, size, "%s", "");
    }
    return snprintf(out, size, "D %u %s\n", (unsigned) row->day, row->name);
}

bool game_status_list_set_day (game_status_list_t *list, const char *name, unsigned day) {
    int i = find(list, name);
    if (i < 0 || !beaten(list->rows[i].status) || day == 0 || day > 0xFFFF) {
        return false;
    }
    list->rows[i].day = (uint16_t) day;
    return true;
}

static int year_days (int year) {
    return ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 366 : 365;
}

int game_status_day_year (unsigned day) {
    int year = 2000;
    while (day >= (unsigned) year_days(year)) {
        day -= (unsigned) year_days(year);
        year++;
    }
    return year;
}

int game_status_list_beaten_in (const game_status_list_t *list, int year, const game_status_row_t **out, int max) {
    int n = 0;
    for (int i = 0; i < list->count; i++) {
        const game_status_row_t *row = &list->rows[i];
        if (beaten(row->status) && row->day && game_status_day_year(row->day) == year) {
            if (out && n < max) {
                out[n] = row;
            }
            n++;
        }
    }
    return n;
}

int game_status_list_beaten_years (const game_status_list_t *list, int *years, int max) {
    int n = 0;
    for (int i = 0; i < list->count; i++) {
        const game_status_row_t *row = &list->rows[i];
        if (!beaten(row->status) || !row->day) {
            continue;
        }
        int y = game_status_day_year(row->day);
        bool seen = false;
        for (int j = 0; j < n && !seen; j++) {
            seen = (years[j] == y);
        }
        if (seen || n >= max) {
            continue;
        }
        int at = n++;
        while (at > 0 && years[at - 1] < y) {
            years[at] = years[at - 1];
            at--;
        }
        years[at] = y;
    }
    return n;
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
