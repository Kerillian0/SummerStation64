/**
 * @file review_scores.c
 * @brief A table of review scores per game code (review_scores.h).
 */

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "review_scores.h"

#define HEADER  "reviewscores 1"

typedef struct {
    char code[4];
    uint8_t score;      /* tenths, 1 to 100 */
    uint16_t line;      /* where it was in the file, so the first of two wins */
} score_t;

static score_t *table = NULL;
static int count = 0;

void review_scores_clear (void) {
    free(table);
    table = NULL;
    count = 0;
}

int review_scores_count (void) {
    return count;
}

/* "87" (tenths) or "8.7" (out of 10) as tenths, or 0 if it is neither or
   outside 0.1 to 10. */
static int parse_score (const char *s) {
    int whole = 0, digits = 0;
    while (isdigit((unsigned char) *s) && digits < 4) {
        whole = whole * 10 + (*s++ - '0');
        digits++;
    }
    if (digits == 0) {
        return 0;
    }
    int tenths = whole;
    if (*s == '.') {
        s++;
        if (!isdigit((unsigned char) *s) || whole > 10) {
            return 0;
        }
        tenths = whole * 10 + (*s++ - '0');
    }
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s != '\0' && *s != '#') {
        return 0;       /* something more after the score */
    }
    return (tenths >= 1 && tenths <= 100) ? tenths : 0;
}

/* One line: a four-letter code, spaces, a score. */
static bool parse_line (char *line, char code[4], int *score) {
    char *s = line;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    for (int i = 0; i < 4; i++) {
        if (!isalnum((unsigned char) s[i])) {
            return false;
        }
        code[i] = (char) toupper((unsigned char) s[i]);
    }
    s += 4;
    if (*s != ' ' && *s != '\t') {
        return false;
    }
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    *score = parse_score(s);
    return *score > 0;
}

static int compare_code_then_line (const void *pa, const void *pb) {
    const score_t *a = pa, *b = pb;
    int order = memcmp(a->code, b->code, 4);
    return order ? order : (int) a->line - (int) b->line;
}

static int compare_code (const void *key, const void *entry) {
    return memcmp(key, ((const score_t *) entry)->code, 4);
}

bool review_scores_read (FILE *f) {
    review_scores_clear();

    char line[128];
    if (!fgets(line, sizeof(line), f) || strncmp(line, HEADER, strlen(HEADER)) != 0) {
        return false;
    }

    int room = 0;
    int line_number = 1;
    while (fgets(line, sizeof(line), f)) {
        line_number++;
        size_t length = strlen(line);
        if (length > 0 && line[length - 1] != '\n' && !feof(f)) {
            /* Longer than any line of the file should be: skip the rest of it. */
            int c;
            while ((c = fgetc(f)) != EOF && c != '\n') {
            }
            continue;
        }
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }

        char code[4];
        int score;
        if (line[0] == '#' || !parse_line(line, code, &score)) {
            continue;
        }
        if (count == room) {
            if (room >= REVIEW_SCORES_MAX) {
                break;
            }
            int more = room ? room * 2 : 256;
            if (more > REVIEW_SCORES_MAX) {
                more = REVIEW_SCORES_MAX;
            }
            score_t *bigger = realloc(table, (size_t) more * sizeof(score_t));
            if (!bigger) {
                break;      /* keep what fitted */
            }
            table = bigger;
            room = more;
        }
        memcpy(table[count].code, code, 4);
        table[count].score = (uint8_t) score;
        table[count].line = (uint16_t) (line_number < 65535 ? line_number : 65535);
        count++;
    }

    /* Sorted by code for a binary search; of a code listed twice, the
       first stays and the rest go. */
    qsort(table, count, sizeof(score_t), compare_code_then_line);
    int kept = 0;
    for (int i = 0; i < count; i++) {
        if (kept == 0 || memcmp(table[kept - 1].code, table[i].code, 4) != 0) {
            table[kept++] = table[i];
        }
    }
    count = kept;
    return true;
}

int review_scores_find (const char *code) {
    if (!code || count == 0) {
        return 0;
    }
    const score_t *found = bsearch(code, table, count, sizeof(score_t), compare_code);
    return found ? found->score : 0;
}
