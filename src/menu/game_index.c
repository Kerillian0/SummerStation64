/**
 * @file game_index.c
 * @brief A remembered list of which file is which game.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <libdragon.h>

#include "game_index.h"
#include "hack_games.h"
#include "safe_file.h"

#define INDEX_PATH      "sd:/menu/gameindex.txt"
#define INDEX_TMP_PATH  "sd:/menu/gameindex.tmp"
#define INDEX_HEADER    "gameindex 1"
#define MAX_GAMES       (768)       /* 15 KB of memory, plus 3.5 KB for developers' names; the oldest notes are written over after that */

#define FLAG_FACTS      (1 << 0)
#define FLAG_NEEDS      (1 << 1)
#define FLAG_LIKES      (1 << 2)
#define FLAG_SAVES      (1 << 3)
#define FLAG_DETAILS    (1 << 4)    /* the metadata pack was asked for this game's date */
#define FLAG_MAKER      (1 << 5)    /* ... and for its developer */

/* A note's paks: GAME_INDEX_PAK_... bits, and this one once they were looked
   for. Lists saved before then have none, so those games are looked up again. */
#define PAKS_KNOWN      (1 << 7)

/* Developers' names are kept once each; a game's note holds the number of
   its developer. About 120 different ones in a full metadata pack. */
#define MAX_MAKERS      (224)
#define MAKER_TEXT      (3072)

typedef struct {
    uint32_t key;       /* made from the file's full path; 0 = empty */
    uint32_t size;      /* the file's size, 0 if it was not known */
    char code[4];
    int8_t players;
    uint8_t flags;
    uint16_t date;      /* release date (see GAME_INDEX_DATE), 0 if not known */
    uint8_t maker;      /* which developer (1 = the first name kept), 0 if not known */
    uint8_t paks;       /* accessories (see PAKS_KNOWN); fits a spare byte, so a note is still 20 bytes */
} note_t;

static char maker_text[MAKER_TEXT];
static uint16_t maker_at[MAX_MAKERS];
static int maker_count = 0;
static int maker_text_used = 0;

/* The number for this developer's name, keeping the name if it is new.
   0 if there is no name or no room left. */
static uint8_t maker_number (const char *name) {
    if (!name || !name[0]) {
        return 0;
    }
    for (int i = 0; i < maker_count; i++) {
        if (strcasecmp(&maker_text[maker_at[i]], name) == 0) {
            return (uint8_t) (i + 1);
        }
    }
    int length = (int) strlen(name) + 1;
    if (maker_count >= MAX_MAKERS || maker_text_used + length > MAKER_TEXT) {
        return 0;
    }
    maker_at[maker_count] = (uint16_t) maker_text_used;
    memcpy(&maker_text[maker_text_used], name, length);
    maker_text_used += length;
    return (uint8_t) (++maker_count);
}

/* Spaces and line ends off both ends, in place. */
static char *trimmed (char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    int length = (int) strlen(text);
    while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r' || text[length - 1] == '\n')) {
        text[--length] = '\0';
    }
    return text;
}

static note_t notes[MAX_GAMES];
static int count = 0;
static int next = 0;        /* where the next new note goes once the list is full */
static bool loaded = false;
static bool dirty = false;
static game_index_entry_t answer;

static uint32_t path_key (const char *path) {
    uint32_t h = 2166136261u;
    for (const unsigned char *c = (const unsigned char *) path; *c; c++) {
        h = (h ^ *c) * 16777619u;
    }
    return h ? h : 1;
}

static bool code_ok (const char *code) {
    for (int i = 0; i < 4; i++) {
        if (!isalnum((unsigned char) code[i])) {
            return false;
        }
    }
    return true;
}

/* Homebrew tools often leave the code's last place blank ("NED "), which
   the list can't keep: such a code is kept with a 0 there. It still reads as
   homebrew, whose art goes by title. */
static const char *kept_code (const char *code, char *fixed) {
    if (!game_index_is_homebrew(code)) {
        return code;
    }
    for (int i = 0; i < 4; i++) {
        fixed[i] = isalnum((unsigned char) code[i]) ? code[i] : '0';
    }
    return fixed;
}

static void load (void) {
    if (loaded) {
        return;
    }
    loaded = true;

    /* If power was cut between remove and rename, the temp file is the good copy. */
    FILE *f = fopen(INDEX_PATH, "r");
    if (!f) {
        f = fopen(INDEX_TMP_PATH, "r");
    }
    if (!f) {
        return;
    }

    char line[128];
    if (fgets(line, sizeof(line), f) && strncmp(line, INDEX_HEADER, strlen(INDEX_HEADER)) == 0) {
        while (count < MAX_GAMES && fgets(line, sizeof(line), f)) {
            unsigned long key, size;
            char code[8];
            int players, flags;
            unsigned int date = 0;      /* lines saved before dates were noted have none */
            int used = 0;               /* the developer's name, if any, is the rest of the line */
            if (sscanf(line, "%lx %lu %4s %d %d %u%n", &key, &size, code, &players, &flags, &date, &used) >= 5 && key != 0 && strlen(code) == 4 && code_ok(code)) {
                note_t *n = &notes[count++];
                n->key = (uint32_t) key;
                n->size = (uint32_t) size;
                memcpy(n->code, code, 4);
                n->players = (int8_t) players;
                n->flags = (uint8_t) flags;
                n->paks = (uint8_t) (flags >> 8);     /* saved in the flags' second byte */
                n->date = (uint16_t) date;
                n->maker = (used > 0) ? maker_number(trimmed(line + used)) : 0;
            }
        }
    }
    fclose(f);
    next = count % MAX_GAMES;
    debugf("game index: %d games remembered\n", count);
}

static note_t *find (uint32_t key, int64_t size) {
    for (int i = 0; i < count; i++) {
        if (notes[i].key == key) {
            /* Same name but another size: the file was replaced. */
            if (size > 0 && notes[i].size != 0 && notes[i].size != (uint32_t) size) {
                return NULL;
            }
            return &notes[i];
        }
    }
    return NULL;
}

/* The note for this file: the one it has, or a fresh one. */
static note_t *note_for (const char *path, int64_t size) {
    load();
    uint32_t key = path_key(path);

    for (int i = 0; i < count; i++) {
        if (notes[i].key == key) {
            if (size > 0 && notes[i].size != 0 && notes[i].size != (uint32_t) size) {
                memset(&notes[i], 0, sizeof(note_t));   /* replaced file: start again */
                notes[i].key = key;
            }
            return &notes[i];
        }
    }

    note_t *n;
    if (count < MAX_GAMES) {
        n = &notes[count++];
    } else {
        n = &notes[next];
        next = (next + 1) % MAX_GAMES;
    }
    memset(n, 0, sizeof(note_t));
    n->key = key;
    return n;
}

const game_index_entry_t *game_index_find (const char *path, int64_t size) {
    load();
    note_t *n = find(path_key(path), size);
    if (!n || !code_ok(n->code)) {
        return NULL;
    }
    /* A hack given a code of its own since it was noted: learn it again under that code. */
    char own[4];
    if (hack_games_code(path, size, own) && memcmp(own, n->code, 4) != 0) {
        memcpy(n->code, own, 4);
        n->flags &= ~(FLAG_FACTS | FLAG_DETAILS | FLAG_MAKER);
        n->paks &= ~PAKS_KNOWN;
        dirty = true;
    }
    memcpy(answer.code, n->code, 4);
    answer.has_facts = (n->flags & FLAG_FACTS) && (n->paks & PAKS_KNOWN);
    answer.players = n->players;
    answer.needs_expansion = (n->flags & FLAG_NEEDS) != 0;
    answer.likes_expansion = (n->flags & FLAG_LIKES) != 0;
    answer.saves = (n->flags & FLAG_SAVES) != 0;
    answer.paks = n->paks & ~PAKS_KNOWN;
    answer.has_details = (n->flags & FLAG_DETAILS) && (n->flags & FLAG_MAKER);
    answer.date = n->date;
    answer.developer = (n->maker > 0 && n->maker <= maker_count) ? &maker_text[maker_at[n->maker - 1]] : NULL;
    return &answer;
}

void game_index_set_code (const char *path, int64_t size, const char *code) {
    char fixed[4];
    code = kept_code(code, fixed);
    if (!code_ok(code)) {
        return;
    }
    note_t *n = note_for(path, size);
    if (memcmp(n->code, code, 4) == 0 && (n->size != 0 || size <= 0)) {
        return;     /* nothing new */
    }
    memcpy(n->code, code, 4);
    if (size > 0) {
        n->size = (uint32_t) size;
    }
    dirty = true;
}

void game_index_set_facts (const char *path, int64_t size, const char *code, int players, bool needs_expansion, bool likes_expansion, bool saves, uint8_t paks) {
    char fixed[4];
    code = kept_code(code, fixed);
    if (!code_ok(code)) {
        return;
    }
    note_t *n = note_for(path, size);
    memcpy(n->code, code, 4);
    if (size > 0) {
        n->size = (uint32_t) size;
    }
    n->players = (int8_t) players;
    n->flags = (n->flags & (FLAG_DETAILS | FLAG_MAKER)) | FLAG_FACTS | (needs_expansion ? FLAG_NEEDS : 0) | (likes_expansion ? FLAG_LIKES : 0) | (saves ? FLAG_SAVES : 0);
    n->paks = PAKS_KNOWN | paks;
    dirty = true;
}

void game_index_set_details (const char *path, int64_t size, uint16_t date, const char *developer) {
    note_t *n = note_for(path, size);
    uint8_t maker = maker_number(developer);
    uint8_t looked = FLAG_DETAILS | FLAG_MAKER;
    if ((n->flags & looked) == looked && n->date == date && n->maker == maker) {
        return;     /* nothing new */
    }
    n->flags |= looked;
    n->date = date;
    n->maker = maker;
    dirty = true;
}

/* If the line is "key = value", the value with the spaces round it removed. */
static char *line_value (char *line, const char *key) {
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    size_t length = strlen(key);
    if (strncmp(line, key, length) != 0) {
        return NULL;
    }
    line += length;
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    return (*line == '=') ? trimmed(line + 1) : NULL;
}

void game_index_read_details (const char *metadata_ini, uint16_t *date, char *developer) {
    *date = 0;
    developer[0] = '\0';

    FILE *f = fopen(metadata_ini, "r");
    if (!f) {
        return;
    }
    /* Only two lines are wanted, so the file is not parsed in full. */
    bool have_date = false;
    bool have_developer = false;
    char line[160];
    while (!(have_date && have_developer) && fgets(line, sizeof(line), f)) {
        char *value;
        if ((value = line_value(line, "release-date"))) {
            int year, month, day;
            if (sscanf(value, "%d-%d-%d", &year, &month, &day) == 3 &&
                year >= 1981 && year <= 2107 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
                *date = GAME_INDEX_DATE(year, month, day);
            }
            have_date = true;
        } else if ((value = line_value(line, "author"))) {
            /* "Developer | Publisher": keep the developer. */
            char *bar = strchr(value, '|');
            if (bar) {
                *bar = '\0';
            }
            int length = 0;
            for (unsigned char *ch = (unsigned char *) value; *ch && length < GAME_INDEX_DEVELOPER_LENGTH - 1; ch++) {
                if (ch[0] == 0xC2 && ch[1] == 0xA0) {   /* a no-break space, which some entries use */
                    developer[length++] = ' ';
                    ch++;
                } else {
                    developer[length++] = (char) *ch;
                }
            }
            developer[length] = '\0';
            char *tidy = trimmed(developer);
            memmove(developer, tidy, strlen(tidy) + 1);
            have_developer = true;
        } else if (line[0] == '[' && strncmp(line, "[meta]", 6) != 0) {
            break;      /* past the section they live in */
        }
    }
    fclose(f);
}

#define ROM_HEADER_SIZE     (0x40)
#define ROM_TITLE_OFFSET    (0x20)
#define ROM_CODE_OFFSET     (0x3B)

/* ROM dumps come in three byte orders; put the header back in the native one. */
static void fix_header_byte_order (uint8_t *h) {
    if (h[0] == 0x37 && h[1] == 0x80) {         /* byte swapped (.v64) */
        for (int i = 0; i < ROM_HEADER_SIZE; i += 2) {
            uint8_t t = h[i]; h[i] = h[i + 1]; h[i + 1] = t;
        }
    } else if (h[0] == 0x40 && h[1] == 0x12) {  /* little endian (.n64) */
        for (int i = 0; i < ROM_HEADER_SIZE; i += 4) {
            uint8_t t = h[i]; h[i] = h[i + 3]; h[i + 3] = t;
            t = h[i + 1]; h[i + 1] = h[i + 2]; h[i + 2] = t;
        }
    }
}

bool game_index_read_header (const char *path, char *code, char *title) {
    uint8_t header[ROM_HEADER_SIZE];
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    size_t read = fread(header, 1, sizeof(header), f);
    fclose(f);
    if (read != sizeof(header)) {
        return false;
    }
    fix_header_byte_order(header);
    memcpy(title, &header[ROM_TITLE_OFFSET], GAME_INDEX_TITLE_LENGTH);
    title[GAME_INDEX_TITLE_LENGTH] = '\0';
    memcpy(code, &header[ROM_CODE_OFFSET], 4);
    hack_games_code(path, 0, code);     /* a hack with a code of its own (hacks.txt) */
    return true;
}

bool game_index_changed (void) {
    return dirty;
}

/* Written to a temporary file first, so a power cut can't leave half a file. */
void game_index_flush (void) {
    if (!dirty) {
        return;
    }
    dirty = false;

    FILE *f = fopen(INDEX_TMP_PATH, "w");
    if (!f) {
        debugf("game index: could not write %s\n", INDEX_TMP_PATH);
        return;
    }
    fprintf(f, "%s\n", INDEX_HEADER);
    for (int i = 0; i < count; i++) {
        note_t *n = &notes[i];
        if (n->key == 0 || !code_ok(n->code)) {
            continue;
        }
        fprintf(f, "%08lx %lu %c%c%c%c %d %d %u", (unsigned long) n->key, (unsigned long) n->size,
            n->code[0], n->code[1], n->code[2], n->code[3], (int) n->players, (int) (n->flags | (n->paks << 8)), (unsigned int) n->date);
        if (n->maker > 0 && n->maker <= maker_count) {
            fprintf(f, " %s", &maker_text[maker_at[n->maker - 1]]);
        }
        fprintf(f, "\n");
    }
    fclose(f);

    if (!safe_file_replace(INDEX_TMP_PATH, INDEX_PATH)) {
        debugf("game index: could not replace %s\n", INDEX_PATH);
    } else {
        debugf("game index: saved %d games\n", count);
    }
}
