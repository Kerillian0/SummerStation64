#include <stdio.h>
#include <string.h>
#include <time.h>

#include "play_years.h"

#define FIRST_YEAR  (1990)  /* anything earlier is a clock that was never set */
#define LAST_YEAR   (2200)

bool play_years_parse (const char *line, play_year_t *out) {
    unsigned long key, year, starts, seconds;
    char rest;
    if (sscanf(line, "y %lx %lu %lu %lu %c", &key, &year, &starts, &seconds, &rest) != 4) {
        return false;
    }
    if (year < FIRST_YEAR || year > LAST_YEAR || key > 0xFFFFFFFFul || seconds > 0xFFFFFFFFul) {
        return false;
    }
    out->key = (uint32_t) key;
    out->year = (uint16_t) year;
    out->starts = (uint16_t) ((starts > 0xFFFF) ? 0xFFFF : starts);
    out->seconds = (uint32_t) seconds;
    return true;
}

int play_years_format (const play_year_t *row, char *out, size_t size) {
    return snprintf(out, size, "y %08lx %u %u %lu\n", (unsigned long) row->key, (unsigned) row->year,
        (unsigned) row->starts, (unsigned long) row->seconds);
}

play_year_t *play_years_find (play_years_t *t, uint32_t key, int year) {
    for (int i = 0; i < t->count; i++) {
        if (t->rows[i].key == key && t->rows[i].year == year) {
            return &t->rows[i];
        }
    }
    return NULL;
}

play_year_t *play_years_add (play_years_t *t, uint32_t key, int year) {
    play_year_t *row = play_years_find(t, key, year);
    if (row) {
        return row;
    }
    if (t->max <= 0) {
        return NULL;
    }
    if (t->count < t->max) {
        row = &t->rows[t->count++];
    } else {
        /* Full: the earliest year's least-played game makes way. */
        row = &t->rows[0];
        for (int i = 1; i < t->count; i++) {
            play_year_t *r = &t->rows[i];
            if (r->year < row->year || (r->year == row->year && r->seconds < row->seconds)) {
                row = r;
            }
        }
    }
    row->key = key;
    row->year = (uint16_t) year;
    row->starts = 0;
    row->seconds = 0;
    return row;
}

void play_years_load_row (play_years_t *t, const play_year_t *in) {
    play_year_t *row = play_years_add(t, in->key, in->year);
    if (!row) {
        return;
    }
    uint32_t starts = (uint32_t) row->starts + in->starts;
    row->starts = (uint16_t) ((starts > 0xFFFF) ? 0xFFFF : starts);
    row->seconds += in->seconds;
}

void play_years_forget (play_years_t *t, uint32_t key) {
    int kept = 0;
    for (int i = 0; i < t->count; i++) {
        if (t->rows[i].key != key) {
            t->rows[kept++] = t->rows[i];
        }
    }
    t->count = kept;
}

void play_years_rekey (play_years_t *t, uint32_t from, uint32_t to) {
    for (int i = 0; i < t->count; i++) {
        if (t->rows[i].key == from) {
            t->rows[i].key = to;
        }
    }
}

int play_years_list (const play_years_t *t, int *years, int max) {
    int n = 0;
    for (int i = 0; i < t->count; i++) {
        int y = t->rows[i].year;
        bool seen = false;
        for (int j = 0; j < n && !seen; j++) {
            seen = (years[j] == y);
        }
        if (seen || n >= max) {
            continue;
        }
        /* Kept newest first as they are found. */
        int at = n++;
        while (at > 0 && years[at - 1] < y) {
            years[at] = years[at - 1];
            at--;
        }
        years[at] = y;
    }
    return n;
}

void play_years_totals (const play_years_t *t, int year, uint32_t *seconds, uint32_t *starts, int *games) {
    *seconds = 0;
    *starts = 0;
    *games = 0;
    for (int i = 0; i < t->count; i++) {
        if (t->rows[i].year != year) {
            continue;
        }
        *seconds += t->rows[i].seconds;
        *starts += t->rows[i].starts;
        if (t->rows[i].starts > 0 || t->rows[i].seconds > 0) {
            (*games)++;
        }
    }
}

int play_years_of (long when) {
    if (when <= 0) {
        return 0;
    }
    time_t t = (time_t) when;
    struct tm *local = localtime(&t);
    if (!local) {
        return 0;
    }
    int year = local->tm_year + 1900;
    return (year < FIRST_YEAR || year > LAST_YEAR) ? 0 : year;
}
