/**
 * @file prep.c
 * @brief "Prepare SD card" (see prep.h).
 */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "http.h"
#include "prep.h"
#include "ra_web.h"
#include "rcheevos/src/util/md5.h"

#define PIECE       (1024 * 1024)
#define PATH_MAX_W  (1024)

typedef struct {
    uint64_t size;
    char md5[33];
    uint32_t game;
    char path[PATH_MAX_W];      /* from the card's top, UTF-8, '/' between folders */
    bool known;                 /* from the last index, not yet seen on the card */
} rom_t;

static rom_t *roms = NULL;
static int rom_count = 0, rom_room = 0;
static wchar_t card[PATH_MAX_W];    /* the card's top, ending in '\' */

static rom_t *add_rom (void) {
    if (rom_count == rom_room) {
        int room = rom_room ? rom_room * 2 : 256;
        rom_t *bigger = realloc(roms, room * sizeof(rom_t));
        if (!bigger) {
            return NULL;
        }
        roms = bigger;
        rom_room = room;
    }
    memset(&roms[rom_count], 0, sizeof(rom_t));
    return &roms[rom_count++];
}

static void to_utf8 (const wchar_t *in, char *out, int size) {
    if (WideCharToMultiByte(CP_UTF8, 0, in, -1, out, size, NULL, NULL) == 0) {
        out[0] = '\0';
    }
}

static void to_wide (const char *in, wchar_t *out, int size) {
    if (MultiByteToWideChar(CP_UTF8, 0, in, -1, out, size) == 0) {
        out[0] = L'\0';
    }
}

/* A path on the card from one relative to its top ('/' or '\'). */
static void card_path (const char *relative, wchar_t *out) {
    wchar_t rel[PATH_MAX_W];
    to_wide(relative, rel, PATH_MAX_W);
    for (wchar_t *c = rel; *c; c++) {
        if (*c == L'/') {
            *c = L'\\';
        }
    }
    _snwprintf(out, PATH_MAX_W, L"%ls%ls", card, rel);
    out[PATH_MAX_W - 1] = L'\0';
}

static bool make_folder (const char *relative) {
    wchar_t path[PATH_MAX_W];
    card_path(relative, path);
    return CreateDirectoryW(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static bool file_exists (const char *relative) {
    wchar_t path[PATH_MAX_W];
    card_path(relative, path);
    DWORD a = GetFileAttributesW(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* Written beside the old one, then swapped in: a pulled card never holds half a file. */
static bool write_file (const char *relative, const void *data, size_t length) {
    wchar_t path[PATH_MAX_W], temp[PATH_MAX_W + 8];
    card_path(relative, path);
    _snwprintf(temp, PATH_MAX_W + 8, L"%ls.tmp", path);
    FILE *f = _wfopen(temp, L"wb");
    if (!f) {
        return false;
    }
    bool ok = fwrite(data, 1, length, f) == length;
    ok = (fclose(f) == 0) && ok;
    if (!ok || !MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(temp);
        return false;
    }
    return true;
}

/* A growing text, for the files written whole. */
typedef struct { char *text; size_t length, room; } text_t;

static void text_add (text_t *t, const char *s) {
    size_t n = strlen(s);
    if (t->length + n + 1 > t->room) {
        size_t room = (t->room ? t->room * 2 : 4096) + n;
        char *bigger = realloc(t->text, room);
        if (!bigger) {
            return;
        }
        t->text = bigger;
        t->room = room;
    }
    memcpy(t->text + t->length, s, n + 1);
    t->length += n;
}

/* Text as one field: no tabs or line breaks. */
static void text_field (text_t *t, const char *s) {
    char piece[2] = { 0, 0 };
    for (; s && *s; s++) {
        piece[0] = (*s == '\t' || *s == '\r' || *s == '\n') ? ' ' : *s;
        text_add(t, piece);
    }
}

/* RetroAchievements answers a dump it knows but has no set for with a marked
   number ("Unsupported Game Version": 1100010202 is game 10202). */
#define UNSUPPORTED_DUMP(id)    ((id) >= 1000000000UL)

static bool is_rom (const wchar_t *name) {
    if (_wcsicmp(name, L"sc64menu.n64") == 0) {
        return false;   /* the menu itself */
    }
    const wchar_t *dot = wcsrchr(name, L'.');
    return dot && (_wcsicmp(dot, L".z64") == 0 || _wcsicmp(dot, L".n64") == 0 || _wcsicmp(dot, L".v64") == 0);
}

static bool skip_folder (const wchar_t *name, bool top) {
    return name[0] == L'.' || _wcsicmp(name, L"saves") == 0 || (top && _wcsicmp(name, L"menu") == 0)
        || _wcsicmp(name, L"System Volume Information") == 0 || _wcsicmp(name, L"$RECYCLE.BIN") == 0;
}

/* The ROM's RetroAchievements hash: MD5 of the whole file in z64 (big-endian) order. */
static bool hash_rom (const wchar_t *path, char md5_out[33]) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) {
        return false;
    }
    static uint8_t piece[PIECE];
    md5_state_t md5;
    md5_init(&md5);
    int order = -1;     /* 0 z64, 1 v64 (16-bit swapped), 2 n64 (32-bit swapped) */
    size_t n;
    while ((n = fread(piece, 1, PIECE, f)) > 0) {
        if (order < 0) {
            order = (n >= 4 && piece[0] == 0x37 && piece[1] == 0x80) ? 1 : (n >= 4 && piece[0] == 0x40 && piece[1] == 0x12) ? 2 : 0;
        }
        if (order == 1) {
            for (size_t i = 0; i + 1 < n; i += 2) {
                uint8_t b = piece[i]; piece[i] = piece[i + 1]; piece[i + 1] = b;
            }
        } else if (order == 2) {
            for (size_t i = 0; i + 3 < n; i += 4) {
                uint8_t b0 = piece[i], b1 = piece[i + 1];
                piece[i] = piece[i + 3]; piece[i + 1] = piece[i + 2]; piece[i + 2] = b1; piece[i + 3] = b0;
            }
        }
        md5_append(&md5, piece, (int) n);
    }
    bool ok = !ferror(f);
    fclose(f);
    md5_byte_t digest[16];
    md5_finish(&md5, digest);
    for (int i = 0; i < 16; i++) {
        snprintf(md5_out + i * 2, 3, "%02x", digest[i]);
    }
    return ok;
}

static rom_t *find_known (const char *path, uint64_t size) {
    for (int i = 0; i < rom_count; i++) {
        if (roms[i].known && roms[i].size == size && strcmp(roms[i].path, path) == 0) {
            return &roms[i];
        }
    }
    return NULL;
}

static void load_index (void) {
    wchar_t path[PATH_MAX_W];
    card_path("menu/achievements/index.txt", path);
    FILE *f = _wfopen(path, L"rb");
    if (!f) {
        return;
    }
    char line[PATH_MAX_W + 128];
    if (fgets(line, sizeof(line), f) && strncmp(line, "achindex 1", 10) == 0) {
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            unsigned long long size;
            char md5[40];
            unsigned long game;
            int at = 0;
            if (sscanf(line, "%llu\t%39s\t%lu\t%n", &size, md5, &game, &at) == 3 && at > 0 && strlen(md5) == 32) {
                rom_t *r = add_rom();
                if (r) {
                    r->size = size;
                    snprintf(r->md5, sizeof(r->md5), "%s", md5);
                    r->game = game;
                    snprintf(r->path, sizeof(r->path), "%s", line + at);
                    r->known = true;
                }
            }
        }
    }
    fclose(f);
}

static int scanned = 0, hashed = 0;

/* Every ROM under `folder` (relative, UTF-8, "" for the top). */
static void walk (const char *folder) {
    wchar_t pattern[PATH_MAX_W];
    char spec[PATH_MAX_W];
    snprintf(spec, sizeof(spec), "%s%s*", folder, folder[0] ? "/" : "");
    card_path(spec, pattern);
    WIN32_FIND_DATAW found;
    HANDLE h = FindFirstFileW(pattern, &found);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (wcscmp(found.cFileName, L".") == 0 || wcscmp(found.cFileName, L"..") == 0) {
            continue;
        }
        char name[PATH_MAX_W / 2], relative[PATH_MAX_W];
        to_utf8(found.cFileName, name, sizeof(name));
        snprintf(relative, sizeof(relative), "%s%s%s", folder, folder[0] ? "/" : "", name);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!skip_folder(found.cFileName, folder[0] == '\0')) {
                walk(relative);
            }
            continue;
        }
        if (!is_rom(found.cFileName)) {
            continue;
        }
        uint64_t size = ((uint64_t) found.nFileSizeHigh << 32) | found.nFileSizeLow;
        scanned++;
        rom_t *known = find_known(relative, size);
        if (known) {
            known->known = false;   /* still on the card: kept */
            continue;
        }
        rom_t *r = add_rom();
        if (!r) {
            continue;
        }
        r->size = size;
        snprintf(r->path, sizeof(r->path), "%s", relative);
        wchar_t full[PATH_MAX_W];
        card_path(relative, full);
        printf("  reading %s\n", relative);
        if (!hash_rom(full, r->md5)) {
            printf("    could not read it; left out\n");
            rom_count--;
            continue;
        }
        hashed++;
    } while (FindNextFileW(h, &found));
    FindClose(h);
}

static bool download (const char *url, const char *relative) {
    if (!url || !url[0]) {
        return false;
    }
    if (file_exists(relative)) {
        return true;
    }
    char *body = NULL;
    size_t length = 0;
    int status = 0;
    bool ok = http_request(url, NULL, NULL, &body, &length, &status) && status == 200 && length > 0
              && write_file(relative, body, length);
    free(body);
    return ok;
}

static int badges_fetched = 0, badges_failed = 0;

/* A file from the card in memory (NULL if it isn't there). */
static uint8_t *read_card_file (const char *relative, size_t *length) {
    wchar_t path[PATH_MAX_W];
    card_path(relative, path);
    FILE *f = _wfopen(path, L"rb");
    *length = 0;
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = (size > 0 && size < 4 * 1024 * 1024) ? malloc(size) : NULL;
    if (data && fread(data, 1, size, f) == (size_t) size) {
        *length = size;
    } else {
        free(data);
        data = NULL;
    }
    fclose(f);
    return data;
}

static void put32 (uint8_t *p, uint32_t v) {
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

/* All of a game's pictures in one file, so the menu opens one file instead of
   searching a big folder per badge: "BPK1", the number of pictures, then an
   offset and a size each (big-endian; size 0 = none), then the PNGs. Pictures
   2i and 2i+1 are achievement i's (in set.txt's order), coloured and locked;
   the last is the game's icon. */
static bool write_pak (const char *folder, char **badges, int count) {
    int pictures = count * 2 + 1;
    size_t header = 8 + (size_t) pictures * 8;
    uint8_t *head = calloc(1, header);
    uint8_t **data = calloc(pictures, sizeof(uint8_t *));
    size_t *sizes = calloc(pictures, sizeof(size_t));
    if (!head || !data || !sizes) {
        free(head); free(data); free(sizes);
        return false;
    }
    for (int i = 0; i < pictures; i++) {
        char name[200];
        if (i == pictures - 1) {
            snprintf(name, sizeof(name), "%s/icon.png", folder);
        } else if (badges[i / 2]) {
            snprintf(name, sizeof(name), "%s/badges/%s%s.png", folder, badges[i / 2], (i & 1) ? "_lock" : "");
        } else {
            continue;
        }
        data[i] = read_card_file(name, &sizes[i]);
    }
    memcpy(head, "BPK1", 4);
    put32(head + 4, (uint32_t) pictures);
    size_t at = header, total;
    for (int i = 0; i < pictures; i++) {
        put32(head + 8 + i * 8, data[i] ? (uint32_t) at : 0);
        put32(head + 12 + i * 8, data[i] ? (uint32_t) sizes[i] : 0);
        at += data[i] ? sizes[i] : 0;
    }
    total = at;
    uint8_t *all = malloc(total);
    bool ok = false;
    if (all) {
        memcpy(all, head, header);
        for (int i = 0, o = (int) header; i < pictures; i++) {
            if (data[i]) {
                memcpy(all + o, data[i], sizes[i]);
                o += (int) sizes[i];
            }
        }
        char name[160];
        snprintf(name, sizeof(name), "%s/badges.pak", folder);
        ok = write_file(name, all, total);
        free(all);
    }
    for (int i = 0; i < pictures; i++) {
        free(data[i]);
    }
    free(head); free(data); free(sizes);
    return ok;
}

/* One game's set, logic, icon and badges. */
static bool prepare_game (uint32_t id) {
    rc_api_fetch_game_data_response_t game;
    if (!ra_web_fetch_game(id, &game)) {
        printf("  game %lu: %s\n", (unsigned long) id, ra_web_error());
        return false;
    }
    char folder[64], name[160];
    snprintf(folder, sizeof(folder), "menu/achievements/%lu", (unsigned long) id);
    make_folder(folder);
    snprintf(name, sizeof(name), "%s/badges", folder);
    make_folder(name);

    text_t set = { 0 }, logic = { 0 };
    char number[96];
    text_add(&set, "achset 1\ngame\t");
    snprintf(number, sizeof(number), "%lu\t", (unsigned long) id);
    text_add(&set, number);
    text_field(&set, game.title);
    text_add(&set, "\n");
    text_add(&logic, "achlogic 1\n");
    int count = 0;
    char **badges = calloc(game.num_achievements ? game.num_achievements : 1, sizeof(char *));
    for (uint32_t i = 0; i < game.num_achievements; i++) {
        const rc_api_achievement_definition_t *d = &game.achievements[i];
        if (d->category != RC_ACHIEVEMENT_CATEGORY_PROMOTED || d->id == 101000001UL || !d->definition) {
            continue;
        }
        snprintf(number, sizeof(number), "a\t%lu\t%lu\t%lu\t", (unsigned long) d->id, (unsigned long) d->points,
                 (unsigned long) d->type);
        text_add(&set, number);
        text_field(&set, d->badge_name);
        text_add(&set, "\t");
        text_field(&set, d->title);
        text_add(&set, "\t");
        text_field(&set, d->description);
        text_add(&set, "\n");
        snprintf(number, sizeof(number), "%lu\t", (unsigned long) d->id);
        text_add(&logic, number);
        text_field(&logic, d->definition);
        text_add(&logic, "\n");
        if (badges && d->badge_name && d->badge_name[0]) {
            badges[count] = strdup(d->badge_name);
        }
        count++;

        if (d->badge_name && d->badge_name[0]) {
            char badge[200];
            snprintf(badge, sizeof(badge), "%s/badges/%s.png", folder, d->badge_name);
            bool had = file_exists(badge);
            bool a = download(d->badge_url, badge);
            snprintf(badge, sizeof(badge), "%s/badges/%s_lock.png", folder, d->badge_name);
            bool b = download(d->badge_locked_url, badge);
            if (a && b) {
                badges_fetched += had ? 0 : 1;
            } else {
                badges_failed++;
            }
        }
    }
    snprintf(name, sizeof(name), "%s/icon.png", folder);
    download(game.image_url, name);
    bool packed = badges && write_pak(folder, badges, count);
    for (int i = 0; badges && i < count; i++) {
        free(badges[i]);
    }
    free(badges);

    snprintf(name, sizeof(name), "%s/set.txt", folder);
    bool ok = set.text && write_file(name, set.text, set.length);
    snprintf(name, sizeof(name), "%s/logic.txt", folder);
    ok = logic.text && write_file(name, logic.text, logic.length) && ok;
    printf("  %s: %d achievements%s\n", game.title ? game.title : "?", count,
           !ok ? " (could not write its files)" : !packed ? " (badges not packed: the menu reads them one by one)" : "");
    free(set.text);
    free(logic.text);
    rc_api_destroy_fetch_game_data_response(&game);
    return ok;
}

bool prep_card (const char *root) {
    to_wide(root, card, PATH_MAX_W - 2);
    size_t n = wcslen(card);
    if (n == 0) {
        printf("No card folder given.\n");
        return false;
    }
    if (card[n - 1] != L'\\' && card[n - 1] != L'/') {
        card[n] = L'\\';
        card[n + 1] = L'\0';
    }
    DWORD a = GetFileAttributesW(card);
    if (a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_DIRECTORY)) {
        printf("Can't open %s: is the SD card in the reader?\n", root);
        return false;
    }
    if (!make_folder("menu") || !make_folder("menu/achievements")) {
        printf("Can't write to %s (is the card locked?)\n", root);
        return false;
    }

    printf("Looking for N64 games on %s ...\n", root);
    load_index();
    walk("");
    /* Games no longer on the card leave the index. */
    int kept = 0;
    for (int i = 0; i < rom_count; i++) {
        if (!roms[i].known) {
            roms[kept++] = roms[i];
        }
    }
    rom_count = kept;
    printf("%d games found, %d read now (the rest were known).\n", scanned, hashed);

    /* Which RetroAchievements game each one is. */
    printf("Asking RetroAchievements which games have achievements...\n");
    for (int i = 0; i < rom_count; i++) {
        if (roms[i].game != 0) {
            continue;
        }
        uint32_t id = 0;
        if (!ra_web_resolve_hash(roms[i].md5, &id)) {
            printf("  %s: %s\n", roms[i].path, ra_web_error());
            continue;
        }
        roms[i].game = id;
        Sleep(50);
    }

    /* Each game's set once, however many copies of it the card holds. */
    int games = 0, failed = 0, other_dumps = 0;
    for (int i = 0; i < rom_count; i++) {
        if (UNSUPPORTED_DUMP(roms[i].game)) {
            printf("  %s: RetroAchievements has this game, but not this dump\n", roms[i].path);
            other_dumps++;
            continue;
        }
        bool first = roms[i].game != 0;
        for (int j = 0; j < i && first; j++) {
            first = (roms[j].game != roms[i].game);
        }
        if (!first) {
            continue;
        }
        games++;
        if (!prepare_game(roms[i].game)) {
            failed++;
        }
    }

    text_t index = { 0 };
    text_add(&index, "achindex 1\n");
    int with = 0;
    for (int i = 0; i < rom_count; i++) {
        char line[PATH_MAX_W + 128];
        snprintf(line, sizeof(line), "%llu\t%s\t%lu\t%s\n", (unsigned long long) roms[i].size, roms[i].md5,
                 (unsigned long) roms[i].game, roms[i].path);
        text_add(&index, line);
        with += (roms[i].game && !UNSUPPORTED_DUMP(roms[i].game)) ? 1 : 0;
    }
    bool ok = index.text && write_file("menu/achievements/index.txt", index.text, index.length);
    free(index.text);

    printf("\nDone: %d of %d games have achievements (%d sets", with, rom_count, games);
    if (failed) {
        printf(", %d couldn't be downloaded", failed);
    }
    printf("); %d badges downloaded", badges_fetched);
    if (badges_failed) {
        printf(", %d couldn't be", badges_failed);
    }
    printf(".\n");
    if (other_dumps) {
        printf("%d games are a dump RetroAchievements doesn't support: its page for each game lists the dumps it does.\n", other_dumps);
    }
    printf(ok ? "The card is ready: put it back in the SummerCart64.\n" : "Could not write menu/achievements/index.txt.\n");
    return ok && failed == 0;
}
