/* PC-side tests for the review scores file reader (src/menu/review_scores.c).
   Run with tests/run.sh. */

#include <stdio.h>
#include <string.h>

#include "review_scores.h"

static int failures = 0;
static int checks = 0;

static void expect (const char *what, int got, int want) {
    checks++;
    if (got != want) {
        failures++;
        printf("  FAIL: %s: got %d, want %d\n", what, got, want);
    }
}

/* Read a file made of this text. */
static bool read_text (const char *text) {
    FILE *f = tmpfile();
    if (!f) {
        printf("  FAIL: no temporary file\n");
        failures++;
        return false;
    }
    fputs(text, f);
    rewind(f);
    bool ok = review_scores_read(f);
    fclose(f);
    return ok;
}

int main (void) {
    printf("a normal file\n");
    expect("read", read_text(
        "reviewscores 1\n"
        "# code, score\n"
        "NSME 87\n"
        "NZLE 99\n"
        "NCDE 41\n"), true);
    expect("count", review_scores_count(), 3);
    expect("NSME", review_scores_find("NSME"), 87);
    expect("NZLE", review_scores_find("NZLE"), 99);
    expect("NCDE", review_scores_find("NCDE"), 41);
    expect("a code it doesn't have", review_scores_find("NXXE"), 0);
    expect("no code", review_scores_find(NULL), 0);

    printf("scores with a point, out of 10\n");
    read_text("reviewscores 1\nAAAA 8.7\nBBBB 10.0\nCCCC 0.1\nDDDD 10\n");
    expect("8.7", review_scores_find("AAAA"), 87);
    expect("10.0", review_scores_find("BBBB"), 100);
    expect("0.1", review_scores_find("CCCC"), 1);
    expect("10 is tenths: 1.0", review_scores_find("DDDD"), 10);

    printf("scores out of range or not numbers are skipped\n");
    read_text(
        "reviewscores 1\n"
        "AAAA 0\n"
        "BBBB 101\n"
        "CCCC 10.1\n"
        "DDDD 8.\n"
        "EEEE eight\n"
        "FFFF -5\n"
        "GGGG 8.75\n"
        "HHHH 87 more\n"
        "IIII\n"
        "JJJJ 50\n");
    expect("only the good line", review_scores_count(), 1);
    expect("JJJJ", review_scores_find("JJJJ"), 50);

    printf("codes: four letters or digits, any case, nothing joined on\n");
    read_text(
        "reviewscores 1\n"
        "nsme 87\n"
        "ABC 50\n"
        "ABCDE 50\n"
        "AB-D 50\n"
        "  CZLE   75  \n"
        "\tNPWE\t60\n");
    expect("count", review_scores_count(), 3);
    expect("lower case read as upper", review_scores_find("NSME"), 87);
    expect("spaces around", review_scores_find("CZLE"), 75);
    expect("tabs", review_scores_find("NPWE"), 60);

    printf("Windows line endings, comments after the score, no last newline\n");
    read_text("reviewscores 1\r\nNSME 87\r\nNZLE 99 # the best\r\nNCDE 41");
    expect("CRLF", review_scores_find("NSME"), 87);
    expect("comment after", review_scores_find("NZLE"), 99);
    expect("no newline at the end", review_scores_find("NCDE"), 41);

    printf("a code listed twice keeps its first score\n");
    read_text("reviewscores 1\nNSME 87\nNZLE 99\nNSME 12\n");
    expect("count", review_scores_count(), 2);
    expect("first wins", review_scores_find("NSME"), 87);

    printf("a line far too long is skipped, the next one read\n");
    {
        char text[1024];
        strcpy(text, "reviewscores 1\nNSME ");
        for (int i = 0; i < 600; i++) strcat(text, "9");
        strcat(text, "\nNZLE 99\n");
        read_text(text);
        expect("count", review_scores_count(), 1);
        expect("next line", review_scores_find("NZLE"), 99);
    }

    printf("not a scores file\n");
    expect("wrong header", read_text("gamedata 1\nNSME 87\n"), false);
    expect("nothing kept", review_scores_count(), 0);
    expect("empty file", read_text(""), false);
    read_text("reviewscores 1\nNSME 87\n");
    expect("a later wrong file clears the table", read_text("hello\n"), false);
    expect("cleared", review_scores_find("NSME"), 0);

    printf("many codes, sorted and found\n");
    {
        static char text[64 * 1100];
        strcpy(text, "reviewscores 1\n");
        char code[8];
        for (int i = 999; i >= 0; i--) {    /* written in reverse order */
            snprintf(code, sizeof(code), "C%03d", i);
            char line[32];
            snprintf(line, sizeof(line), "%s %d\n", code, (i % 100) + 1);
            strcat(text, line);
        }
        read_text(text);
        expect("count", review_scores_count(), 1000);
        expect("C000", review_scores_find("C000"), 1);
        expect("C512", review_scores_find("C512"), 13);
        expect("C999", review_scores_find("C999"), 100);
    }

    review_scores_clear();
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
