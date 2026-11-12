/* PC-side tests for the status marks' list and file lines (src/menu/game_status_list.c).
   Run with tests/run.sh. */

#include <stdio.h>
#include <string.h>

#include "game_status_list.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static game_status_list_t list;

static void lines (void) {
    game_status_t s;
    char name[64];
    printf("reading lines\n");
    CHECK(game_status_parse("B N64(US)/Super Mario 64 (U) [!].z64\n", &s, name, sizeof(name)), "a Beaten line is read");
    CHECK(s == GAME_STATUS_BEATEN && strcmp(name, "N64(US)/Super Mario 64 (U) [!].z64") == 0, "its status and name (%d, \"%s\")", s, name);
    CHECK(game_status_parse("P Game.z64\r\n", &s, name, sizeof(name)) && s == GAME_STATUS_PLAYING && strcmp(name, "Game.z64") == 0, "Playing, CR LF dropped");
    CHECK(game_status_parse("C a b.n64", &s, name, sizeof(name)) && s == GAME_STATUS_COMPLETE && strcmp(name, "a b.n64") == 0, "100%%, spaces kept, no line end");
    CHECK(game_status_parse("L x.z64\n", &s, name, sizeof(name)) && s == GAME_STATUS_BACKLOG, "Backlog");
    CHECK(!game_status_parse("status 1\n", &s, name, sizeof(name)), "the header is not a game");
    CHECK(!game_status_parse("X Game.z64\n", &s, name, sizeof(name)), "an unknown letter is refused");
    CHECK(!game_status_parse("B\n", &s, name, sizeof(name)), "a letter without a name is refused");
    CHECK(!game_status_parse("B \n", &s, name, sizeof(name)), "an empty name is refused");
    CHECK(!game_status_parse("BGame.z64\n", &s, name, sizeof(name)), "no space after the letter is refused");
    CHECK(!game_status_parse("", &s, name, sizeof(name)), "an empty line is refused");
    char tiny[4];
    CHECK(!game_status_parse("B Game.z64\n", &s, tiny, sizeof(tiny)), "a name too long for the room is refused");

    char out[64];
    game_status_format(GAME_STATUS_COMPLETE, "N64/Game.z64", out, sizeof(out));
    CHECK(strcmp(out, "C N64/Game.z64\n") == 0, "written as \"%s\"", out);
    CHECK(game_status_parse(out, &s, name, sizeof(name)) && s == GAME_STATUS_COMPLETE && strcmp(name, "N64/Game.z64") == 0, "a written line reads back");
    game_status_format(GAME_STATUS_NONE, "N64/Game.z64", out, sizeof(out));
    CHECK(out[0] == '\0', "None writes nothing");
}

static void table (void) {
    printf("the list\n");
    CHECK(game_status_list_get(&list, "A.z64") == GAME_STATUS_NONE, "an unknown game has none");
    CHECK(game_status_list_set(&list, "A.z64", GAME_STATUS_PLAYING), "a status is set");
    CHECK(game_status_list_get(&list, "A.z64") == GAME_STATUS_PLAYING, "and read back");
    CHECK(game_status_list_get(&list, "a.z64") == GAME_STATUS_NONE, "names match exactly");
    game_status_list_set(&list, "A.z64", GAME_STATUS_BEATEN);
    CHECK(list.count == 1 && game_status_list_get(&list, "A.z64") == GAME_STATUS_BEATEN, "setting again changes it, no second row");
    game_status_list_set(&list, "B.z64", GAME_STATUS_BACKLOG);
    game_status_list_set(&list, "C.z64", GAME_STATUS_BACKLOG);
    CHECK(game_status_list_count(&list, GAME_STATUS_BACKLOG) == 2, "two in the backlog");
    game_status_list_set(&list, "B.z64", GAME_STATUS_NONE);
    CHECK(list.count == 2 && game_status_list_get(&list, "B.z64") == GAME_STATUS_NONE, "None takes it off the list");
    CHECK(game_status_list_get(&list, "C.z64") == GAME_STATUS_BACKLOG, "the others stay");

    CHECK(game_status_list_rename(&list, "A.z64", "Folder/A2.z64"), "a rename is followed");
    CHECK(game_status_list_get(&list, "Folder/A2.z64") == GAME_STATUS_BEATEN && game_status_list_get(&list, "A.z64") == GAME_STATUS_NONE, "under its new name only");
    CHECK(!game_status_list_rename(&list, "C.z64", "Folder/A2.z64"), "a rename onto a marked name is refused");
    CHECK(!game_status_list_rename(&list, "nothing.z64", "x.z64"), "renaming an unmarked game changes nothing");

    game_status_list_clear(&list);
    char name[32];
    int set = 0;
    for (int i = 0; i < GAME_STATUS_MAX + 5; i++) {
        snprintf(name, sizeof(name), "G%d.z64", i);
        set += game_status_list_set(&list, name, GAME_STATUS_PLAYING) ? 1 : 0;
    }
    CHECK(set == GAME_STATUS_MAX && list.count == GAME_STATUS_MAX, "the list holds %d (%d set)", GAME_STATUS_MAX, set);
    CHECK(game_status_list_get(&list, "G511.z64") == GAME_STATUS_PLAYING, "the last that fitted is there");
    CHECK(game_status_list_set(&list, "G0.z64", GAME_STATUS_BEATEN), "a full list still changes a game it has");
    game_status_list_clear(&list);
    CHECK(list.count == 0, "cleared");
}

static void days (void) {
    printf("beaten days\n");
    unsigned day;
    char name[64];
    CHECK(game_status_parse_day("D 9447 N64(US)/0222 - F-Zero X.z64\n", &day, name, sizeof(name)) && day == 9447
        && strcmp(name, "N64(US)/0222 - F-Zero X.z64") == 0, "a day line is read, a name starting with digits kept whole (%u, \"%s\")", day, name);
    CHECK(!game_status_parse_day("D Game.z64\n", &day, name, sizeof(name)), "no day is refused");
    CHECK(!game_status_parse_day("D 0 Game.z64\n", &day, name, sizeof(name)), "day 0 is refused");
    CHECK(!game_status_parse_day("D 70000 Game.z64\n", &day, name, sizeof(name)), "a day past what is kept is refused");
    CHECK(!game_status_parse_day("D 12\n", &day, name, sizeof(name)), "no name is refused");
    game_status_t s;
    CHECK(!game_status_parse("D 9447 Game.z64\n", &s, name, sizeof(name)), "a day line is not a status line (older menus skip it)");

    CHECK(game_status_day_year(0) == 2000 && game_status_day_year(365) == 2000 && game_status_day_year(366) == 2001, "2000 has 366 days");
    CHECK(game_status_day_year(9446) == 2025 && game_status_day_year(9447) == 2025 && game_status_day_year(9497) == 2026,
        "2026-01-01 is day 9497 (%d, %d)", game_status_day_year(9496), game_status_day_year(9497));

    game_status_list_clear(&list);
    game_status_list_set_on(&list, "A.z64", GAME_STATUS_PLAYING, 9500);
    CHECK(list.rows[0].day == 0, "Playing has no day");
    game_status_list_set_on(&list, "A.z64", GAME_STATUS_BEATEN, 9510);
    CHECK(list.rows[0].day == 9510, "becoming Beaten dates it");
    game_status_list_set_on(&list, "A.z64", GAME_STATUS_COMPLETE, 9600);
    CHECK(list.rows[0].day == 9510, "Beaten to 100%% keeps the day it was beaten");
    game_status_list_set_on(&list, "A.z64", GAME_STATUS_BACKLOG, 9700);
    CHECK(list.rows[0].day == 0, "another status forgets it");
    game_status_list_set_on(&list, "B.z64", GAME_STATUS_COMPLETE, 9200);
    CHECK(list.rows[1].day == 9200, "a new game straight to 100%% is dated");
    game_status_list_set(&list, "C.z64", GAME_STATUS_BEATEN);
    CHECK(list.rows[2].day == 0, "set without a day (loading) leaves none");
    CHECK(game_status_list_set_day(&list, "C.z64", 9520), "a day line gives a loaded game its day");
    CHECK(!game_status_list_set_day(&list, "A.z64", 9520), "but not to a game that isn't beaten");
    CHECK(!game_status_list_set_day(&list, "Z.z64", 9520), "nor to one not on the list");

    char out[64];
    game_status_format_day(&list.rows[2], out, sizeof(out));
    CHECK(strcmp(out, "D 9520 C.z64\n") == 0, "written as \"%s\"", out);
    game_status_format_day(&list.rows[0], out, sizeof(out));
    CHECK(out[0] == '\0', "a game with no day writes no line");

    game_status_list_set_on(&list, "D.z64", GAME_STATUS_BEATEN, 9497);
    const game_status_row_t *rows[4];
    CHECK(game_status_list_beaten_in(&list, 2026, rows, 4) == 2, "two beaten in 2026 (C, D)");
    CHECK(game_status_list_beaten_in(&list, 2025, rows, 4) == 1 && strcmp(rows[0]->name, "B.z64") == 0, "one in 2025 (B)");
    int years[4];
    int n = game_status_list_beaten_years(&list, years, 4);
    CHECK(n == 2 && years[0] == 2026 && years[1] == 2025, "the years, newest first");
    CHECK(game_status_list_rename(&list, "C.z64", "N64/C.z64") && list.rows[2].day == 9520, "a renamed game keeps its day");
    game_status_list_clear(&list);
}

int main (void) {
    lines();
    table();
    days();
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
