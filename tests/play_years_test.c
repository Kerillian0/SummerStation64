/* PC-side tests for play stats per year (src/menu/play_years.c).
   Run with tests/run.sh. */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "play_years.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void lines (void) {
    play_year_t r;
    printf("reading lines\n");
    CHECK(play_years_parse("y 1a2b3c4d 2026 3 9200\n", &r), "a good line is read");
    CHECK(r.key == 0x1a2b3c4du && r.year == 2026 && r.starts == 3 && r.seconds == 9200, "its numbers: %08x %u %u %u",
        (unsigned) r.key, r.year, r.starts, (unsigned) r.seconds);
    CHECK(play_years_parse("y 1a2b3c4d 2026 3 9200\r\n", &r), "a line ending in CR LF is read");
    CHECK(!play_years_parse("1a2b3c4d 12 1794440146 74400\n", &r), "a game's totals line is not a year line");
    CHECK(!play_years_parse("session 1794440146 sd:/N64/Game.z64\n", &r), "the session line is not a year line");
    CHECK(!play_years_parse("y 1a2b3c4d 2026 3\n", &r), "a short line is refused");
    CHECK(!play_years_parse("y 1a2b3c4d 2026 3 9200 7\n", &r), "a line with more on it is refused");
    CHECK(!play_years_parse("y zz 2026 3 9200\n", &r), "a bad hash is refused");
    CHECK(!play_years_parse("y 1a2b3c4d 1970 3 9200\n", &r), "a year from an unset clock is refused");
    CHECK(!play_years_parse("", &r), "an empty line is refused");
    CHECK(play_years_parse("y 1 2026 99999 5\n", &r) && r.starts == 0xFFFF, "starts over 65535 are held at 65535");

    char out[48];
    play_year_t w = { .key = 0x0000abcd, .year = 2027, .starts = 12, .seconds = 3600 };
    play_years_format(&w, out, sizeof(out));
    CHECK(strcmp(out, "y 0000abcd 2027 12 3600\n") == 0, "written as \"%s\"", out);
    play_year_t back;
    CHECK(play_years_parse(out, &back) && memcmp(&back, &w, sizeof(w)) == 0, "a written line reads back the same");

    /* An older menu reads the totals lines with "%lx %lu %lu %lu" and must skip year lines. */
    unsigned long a, b, c, d;
    CHECK(sscanf("y 1a2b3c4d 2026 3 9200\n", "%lx %lu %lu %lu", &a, &b, &c, &d) < 3, "older menus skip year lines");
}

static void table (void) {
    play_year_t rows[4];
    play_years_t t = { .rows = rows, .count = 0, .max = 4 };
    printf("the table\n");

    play_year_t *r = play_years_add(&t, 1, 2026);
    r->starts = 2; r->seconds = 100;
    CHECK(play_years_add(&t, 1, 2026) == r && t.count == 1, "the same game and year is one row");
    play_years_add(&t, 1, 2027)->seconds = 50;
    play_years_add(&t, 2, 2027)->starts = 1;
    CHECK(t.count == 3, "three rows (%d)", t.count);

    int list[4];
    int n = play_years_list(&t, list, 4);
    CHECK(n == 2 && list[0] == 2027 && list[1] == 2026, "years newest first (%d: %d %d)", n, list[0], list[1]);

    uint32_t seconds, starts; int games;
    play_years_totals(&t, 2027, &seconds, &starts, &games);
    CHECK(seconds == 50 && starts == 1 && games == 2, "2027: %u s, %u starts, %d games", (unsigned) seconds, (unsigned) starts, games);

    play_years_rekey(&t, 1, 9);
    CHECK(play_years_find(&t, 9, 2026) && play_years_find(&t, 9, 2027) && !play_years_find(&t, 1, 2026), "a renamed game keeps its years");

    play_year_t in = { .key = 9, .year = 2026, .starts = 3, .seconds = 7 };
    play_years_load_row(&t, &in);
    CHECK(play_years_find(&t, 9, 2026)->starts == 5 && play_years_find(&t, 9, 2026)->seconds == 107, "a second line for one year adds up");

    play_years_add(&t, 3, 2028);            /* fills the fourth row */
    play_years_add(&t, 4, 2028);            /* full: 2026's row makes way */
    CHECK(t.count == 4 && !play_years_find(&t, 9, 2026) && play_years_find(&t, 4, 2028), "when full, the earliest year makes way");

    play_years_forget(&t, 9);
    CHECK(!play_years_find(&t, 9, 2027) && t.count == 3, "a forgotten game has no years left (%d rows)", t.count);

    play_years_t none = { .rows = NULL, .count = 0, .max = 0 };
    CHECK(play_years_add(&none, 1, 2026) == NULL, "no room at all gives NULL");
}

static void clock_years (void) {
    printf("the year of a time\n");
    struct tm when = { .tm_year = 2026 - 1900, .tm_mon = 10, .tm_mday = 12, .tm_hour = 12 };
    time_t t = mktime(&when);
    CHECK(play_years_of((long) t) == 2026, "Nov 12, 2026 is in 2026 (%d)", play_years_of((long) t));
    CHECK(play_years_of(0) == 0 && play_years_of(-1) == 0, "an unknown time has no year");
    CHECK(play_years_of(86400L * 365) == 0, "1971 (a clock never set) has no year");
}

int main (void) {
    lines();
    table();
    clock_years();
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
