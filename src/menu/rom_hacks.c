/**
 * @file rom_hacks.c
 * @brief Tells which games in a list are hacks of other games.
 */

#include <ctype.h>
#include <string.h>

#include "rom_hacks.h"

#define WORDS_MOST      (16)
#define WORD_LENGTH     (20)
#define GROUP_MOST      (24)

/* Is `word` in `text`, whatever the capitals? */
static bool has_word (const char *text, const char *word) {
    size_t length = strlen(word);
    for (const char *t = text; *t; t++) {
        size_t i = 0;
        while (i < length && t[i] && tolower((unsigned char) t[i]) == word[i]) {
            i++;
        }
        if (i == length) {
            return true;
        }
    }
    return false;
}

/* GoodN64 marks a hacked copy "[h1]", "[h2C]", ...: "[h" and then a digit. */
static bool has_hack_tag (const char *name) {
    for (const char *t = name; t[0] && t[1] && t[2]; t++) {
        if (t[0] == '[' && (t[1] == 'h' || t[1] == 'H') && isdigit((unsigned char) t[2])) {
            return true;
        }
    }
    return false;
}

bool rom_hacks_name_says (const char *file_name) {
    return has_word(file_name, "hack") || has_word(file_name, "redux") || has_hack_tag(file_name);
}

/* The words of a game's name, for comparing names: in small letters,
   without the file ending, the tags in brackets, and little words that
   come and go between namings ("The", "of"); roman I-III as digits. */
typedef struct {
    char word[WORDS_MOST][WORD_LENGTH];
    int count;
} words_t;

static void name_words (const char *name, words_t *out) {
    static const char *const skip[] = { "the", "a", "an", "of", "and", "s" };
    out->count = 0;
    const char *dot = strrchr(name, '.');
    const char *end = dot ? dot : name + strlen(name);
    int depth = 0;
    char word[WORD_LENGTH];
    int length = 0;
    for (const char *c = name; ; c++) {
        bool at_end = (c >= end);
        unsigned char ch = at_end ? ' ' : (unsigned char) *c;
        if (ch == '(' || ch == '[') depth++;
        if (depth == 0 && isalnum(ch)) {
            if (length < WORD_LENGTH - 1) word[length++] = (char) tolower(ch);
        } else if (length > 0) {
            word[length] = '\0';
            length = 0;
            if (!strcmp(word, "i")) strcpy(word, "1");
            else if (!strcmp(word, "ii")) strcpy(word, "2");
            else if (!strcmp(word, "iii")) strcpy(word, "3");
            bool little = false;
            for (int k = 0; k < (int) (sizeof(skip) / sizeof(skip[0])); k++) little = little || !strcmp(word, skip[k]);
            bool seen = false;
            for (int k = 0; k < out->count; k++) seen = seen || !strcmp(out->word[k], word);
            if (!little && !seen && out->count < WORDS_MOST) strcpy(out->word[out->count++], word);
        }
        if ((ch == ')' || ch == ']') && depth > 0) depth--;
        if (at_end) break;
    }
}

/* Has `big` every word of `small`? */
static bool holds (const words_t *big, const words_t *small) {
    for (int i = 0; i < small->count; i++) {
        bool found = false;
        for (int j = 0; j < big->count && !found; j++) found = !strcmp(big->word[j], small->word[i]);
        if (!found) return false;
    }
    return true;
}

void rom_hacks_find (const char *const *names, const char (*codes)[4], const bool *is_rom, int count, bool *hack) {
    static const char unknown[4] = { 0, 0, 0, 0 };
    static words_t words[GROUP_MOST];
    static int score[GROUP_MOST];
    int member[GROUP_MOST];

    for (int i = 0; i < count; i++) {
        hack[i] = is_rom[i] && rom_hacks_name_says(names[i]);
    }

    /* Rule 2, a group of files at a time: the files sharing one game code. */
    for (int i = 0; i < count; i++) {
        if (!is_rom[i] || memcmp(codes[i], unknown, 4) == 0) {
            continue;
        }
        bool first = true;
        int size = 0;
        for (int j = 0; j < count && first; j++) {
            if (is_rom[j] && memcmp(codes[i], codes[j], 4) == 0) {
                if (j < i) first = false;
                else if (size < GROUP_MOST) member[size++] = j;
            }
        }
        if (!first || size < 2) {
            continue;
        }
        for (int m = 0; m < size; m++) {
            name_words(names[member[m]], &words[m]);
        }
        /* The original's words are found again in the other files' names:
           a hack adds words ("Return to Yoshi's Island"), another copy of
           the game has the same ones ("007 - GoldenEye", "GoldenEye 007"). */
        int best = 0;
        for (int m = 0; m < size; m++) {
            score[m] = 0;
            for (int n = 0; n < size; n++) {
                if (n != m && holds(&words[n], &words[m])) score[m]++;
            }
            if (score[m] > best) best = score[m];
        }
        if (best == 0) {
            continue;   /* no name holds another: which one is the original can't be told */
        }
        for (int m = 0; m < size; m++) {
            bool original = false;
            for (int n = 0; n < size && !original; n++) {
                original = score[n] == best && holds(&words[m], &words[n]) && holds(&words[n], &words[m]);
            }
            if (!original) hack[member[m]] = true;
        }
    }
}
