#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

#include "animations.h"
#include "assistant.h"
#include "assistant_ask.h"
#include "fonts.h"
#include "games_ui.h"
#include "sound.h"
#include "theme.h"
#include "ui_components/constants.h"

#define ASK_PATH        "rom:/ask.txt"
#define ASK_HEADER      "ask 1"
#define ASK_MOST_BYTES  (64 * 1024)
#define TOPICS_MAX      (16)
#define QUESTIONS_MAX   (128)
#define ANSWER_LINES_MAX (384)
#define OPENERS_MAX     (12)

/* The same frame as Settings' pages (settings_menu.c). */
#define ROW_HEIGHT      (26)
#define TITLE_Y         (VISIBLE_AREA_Y0 + 10)
#define ROWS_Y          (TITLE_Y + 32)
#define VISIBLE_ROWS    (8)
#define LEFT_X          (VISIBLE_AREA_X0 + TEXT_MARGIN_HORIZONTAL)
#define RIGHT_X1        (VISIBLE_AREA_X1 - TEXT_MARGIN_HORIZONTAL)
#define TOPICS_WIDTH    (176)
#define QUESTIONS_X     (LEFT_X + TOPICS_WIDTH + 12)
#define QUESTIONS_WIDTH (RIGHT_X1 - QUESTIONS_X)
#define PANEL_PAD       (4)
#define ROW_PADDING     (6)
#define LOWER_Y         (ROWS_Y + (VISIBLE_ROWS * ROW_HEIGHT) + PANEL_PAD + 6)     /* the panel along the bottom */
#define TEXT_PAD        (10)
#define EDGE            (3)         /* the answer panel's edge, in the fairy's colour */
#define PERCH           (42)        /* the fairy, perched on the top right corner */
#define TITLE_WIDTH     (176)
#define SCROLL_STEP     (18)

/* ---------- the questions, from rom:/ask.txt ---------- */

typedef struct {
    const char *name;
    int first, count;           /* its questions */
} topic_t;

typedef struct {
    const char *text;
    int first, count;           /* its answer's paragraphs */
} question_t;

static char *data = NULL;
static topic_t topics[TOPICS_MAX];
static question_t questions[QUESTIONS_MAX];
static const char *answer_lines[ANSWER_LINES_MAX];
static const char *openers[2][OPENERS_MAX];     /* [0] eager, [1] snarky */
static int topic_count, question_count, line_count, opener_count[2];

static void forget (void) {
    free(data);
    data = NULL;
    topic_count = question_count = line_count = 0;
    opener_count[0] = opener_count[1] = 0;
}

static bool load (void) {
    forget();
    FILE *f = fopen(ASK_PATH, "r");
    if (!f) {
        debugf("ask: %s not found\n", ASK_PATH);
        return false;
    }
    data = malloc(ASK_MOST_BYTES);
    if (!data) {
        fclose(f);
        return false;
    }
    size_t size = fread(data, 1, ASK_MOST_BYTES - 1, f);
    fclose(f);
    data[size] = '\0';

    /* Cut it into lines in place, and note what each one is. */
    bool header = false;
    for (char *line = data; line && *line; ) {
        char *next = strchr(line, '\n');
        if (next) {
            *next++ = '\0';
        }
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }
        if (!header) {
            header = (strcmp(line, ASK_HEADER) == 0);
            if (!header) {
                debugf("ask: %s is not a version 1 file\n", ASK_PATH);
                forget();
                return false;
            }
        } else if (strncmp(line, "open ", 5) == 0 && (line[5] == 'e' || line[5] == 's') && line[6] == ' ') {
            int mood = (line[5] == 's');
            if (opener_count[mood] < OPENERS_MAX) {
                openers[mood][opener_count[mood]++] = line + 7;
            }
        } else if (strncmp(line, "topic ", 6) == 0 && topic_count < TOPICS_MAX) {
            topics[topic_count++] = (topic_t) { .name = line + 6, .first = question_count, .count = 0 };
        } else if (strncmp(line, "q ", 2) == 0 && topic_count > 0 && question_count < QUESTIONS_MAX) {
            questions[question_count++] = (question_t) { .text = line + 2, .first = line_count, .count = 0 };
            topics[topic_count - 1].count++;
        } else if (strncmp(line, "a ", 2) == 0 && question_count > 0 && line_count < ANSWER_LINES_MAX) {
            answer_lines[line_count++] = line + 2;
            questions[question_count - 1].count++;
        }
        line = next;
    }
    debugf("ask: %d topics, %d questions\n", topic_count, question_count);
    return topic_count > 0;
}

/* ---------- the page ---------- */

typedef enum { LEVEL_TOPICS, LEVEL_QUESTIONS, LEVEL_ANSWER } level_t;

static bool active = false;
static bool opened_from_tabs = false;
static level_t level = LEVEL_TOPICS;
static int topic = 0, question = 0;     /* question: within the topic */
static rdpq_paragraph_t *answer = NULL;
static int answer_height = 0, scroll = 0;
static uint64_t answer_since = 0;
static int opener_next[2];

bool assistant_ask_active (void) {
    return active;
}

static void answer_free (void) {
    if (answer) {
        rdpq_paragraph_free(answer);
        answer = NULL;
    }
}

static int answer_width (void) {
    return (RIGHT_X1 - LEFT_X) + (2 * PANEL_PAD) - (2 * TEXT_PAD) - (2 * EDGE);
}

/* Copy `text` into out, with %n as the fairy's name. */
static size_t put (char *out, size_t at, size_t size, const char *text) {
    const char *name = assistant_name();
    for (const char *c = text; *c && at + 1 < size; c++) {
        if (c[0] == '%' && c[1] == 'n') {
            for (const char *n = name; *n && at + 1 < size; n++) {
                out[at++] = *n;
            }
            c++;
        } else {
            out[at++] = *c;
        }
    }
    out[at] = '\0';
    return at;
}

/* Lay out the chosen question's answer: the fairy's name in orange and a
   line in its mood, then the answer's paragraphs. */
static void answer_build (void) {
    answer_free();
    const question_t *q = &questions[topics[topic].first + question];
    static char text[2048];
    size_t at = (size_t) snprintf(text, sizeof(text), "^%02X%s:^00 ", STL_ORANGE, assistant_name());
    int mood = assistant_snarky() ? 1 : 0;
    if (opener_count[mood] > 0) {
        at = put(text, at, sizeof(text), openers[mood][opener_next[mood]]);
        opener_next[mood] = (opener_next[mood] + 1 + (int) (get_ticks_ms() % 2)) % opener_count[mood];   /* not always the same order */
    }
    for (int i = 0; i < q->count; i++) {
        at = put(text, at, sizeof(text), "\n\n");
        at = put(text, at, sizeof(text), answer_lines[q->first + i]);
    }
    int bytes = (int) at;
    answer = rdpq_paragraph_build(&(rdpq_textparms_t) {
        .width = answer_width(),
        .height = 2000,
        .wrap = WRAP_WORD,
        .style_id = STL_DEFAULT,
    }, FNT_DEFAULT, text, &bytes);
    answer_height = (int) (answer->bbox.y1 - answer->bbox.y0);
    scroll = 0;
    answer_since = get_ticks_ms();
}

void assistant_ask_open (bool from_tabs) {
    if (!load()) {
        return;
    }
    active = true;
    opened_from_tabs = from_tabs;
    level = LEVEL_TOPICS;
    topic = 0;
    question = 0;
}

static void close_page (menu_t *menu) {
    active = false;
    answer_free();
    assistant_portrait_free();
    forget();
    if (opened_from_tabs) {
        menu->next_mode = games_ui_origin();    /* back to the cover tab it was asked from */
    }
}

/* The answer panel's inside height, for scrolling. */
static int answer_view_height (void) {
    int x0, y0, x1, y1;
    games_ui_hints_rect(&x0, &y0, &x1, &y1);
    int bottom = y0 + ((y1 - y0) / 2) - 4;
    return (bottom - EDGE - TEXT_PAD) - (ROWS_Y - PANEL_PAD + EDGE + TEXT_PAD + ROW_HEIGHT);
}

void assistant_ask_process (menu_t *menu) {
    const topic_t *t = &topics[topic];
    switch (level) {
        case LEVEL_TOPICS:
            if (menu->actions.go_up && topic > 0) {
                topic--;
                question = 0;
                sound_play_effect(SFX_CURSOR);
            } else if (menu->actions.go_down && topic < topic_count - 1) {
                topic++;
                question = 0;
                sound_play_effect(SFX_CURSOR);
            } else if ((menu->actions.enter || menu->actions.go_right) && t->count > 0) {
                level = LEVEL_QUESTIONS;
                sound_play_effect(SFX_ENTER);
            } else if (menu->actions.back) {
                close_page(menu);
                sound_play_effect(SFX_EXIT);
            }
            break;
        case LEVEL_QUESTIONS:
            if (menu->actions.go_up && question > 0) {
                question--;
                sound_play_effect(SFX_CURSOR);
            } else if (menu->actions.go_down && question < t->count - 1) {
                question++;
                sound_play_effect(SFX_CURSOR);
            } else if (menu->actions.enter) {
                answer_build();
                level = LEVEL_ANSWER;
                sound_play_effect(SFX_ENTER);
            } else if (menu->actions.back || menu->actions.go_left) {
                level = LEVEL_TOPICS;
                sound_play_effect(SFX_EXIT);
            }
            break;
        case LEVEL_ANSWER: {
            int most = answer_height - answer_view_height();
            if (menu->actions.go_up && scroll > 0) {
                scroll -= SCROLL_STEP;
                if (scroll < 0) {
                    scroll = 0;
                }
            } else if (menu->actions.go_down && scroll < most) {
                scroll += SCROLL_STEP;
            } else if ((menu->actions.go_left && question > 0) || (menu->actions.go_right && question < t->count - 1)) {
                question += menu->actions.go_right ? 1 : -1;    /* the next or previous question, answered */
                answer_build();
                sound_play_effect(SFX_CURSOR);
            } else if (menu->actions.back || menu->actions.enter) {
                answer_free();
                level = LEVEL_QUESTIONS;
                sound_play_effect(SFX_EXIT);
            }
            break;
        }
    }
}

/* ---------- drawing ---------- */

static void text_at (int x, int y, int width, int height, rdpq_align_t align, menu_font_style_t style, const char *text) {
    rdpq_text_printf(&(rdpq_textparms_t) { .width = width, .height = height, .align = align, .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES, .style_id = style }, FNT_DEFAULT, x, y, "%s", text);
}

static void fill (int x0, int y0, int x1, int y1, color_t color) {
    rdpq_mode_push();
        rdpq_set_mode_fill(color);
        rdpq_fill_rectangle(x0, y0, x1, y1);
    rdpq_mode_pop();
}

/* The row being used gets a solid rounded bar; the other column keeps a thin accent underline. */
static void selection (int x, int y, int width, bool focused) {
    if (focused) {
        games_ui_round_fill(x, y, x + width, y + ROW_HEIGHT, FILE_LIST_HIGHLIGHT_COLOR);
    } else {
        fill(x + 4, y + ROW_HEIGHT - 2, x + width - 4, y + ROW_HEIGHT, theme_get()->accent);
    }
}

static int lower_bottom (void) {
    int x0, y0, x1, y1;
    games_ui_hints_rect(&x0, &y0, &x1, &y1);
    return y0 + ((y1 - y0) / 2) - 4;
}

/* One row of hints with the cover rows' icons, as in Settings. */
static void hints (void) {
    int x0, y0, x1, y1;
    games_ui_hints_rect(&x0, &y0, &x1, &y1);
    const theme_t *t = theme_get();
    games_ui_round_fill(x0, y0 + ((y1 - y0) / 2), x1, y1, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8));
    int x = GAMES_UI_HINTS_X;
    if (level == LEVEL_ANSWER) {
        x = games_ui_hint_draw(x, 1, "B", "Back");
        x = games_ui_hint_draw(x, 1, "◀▶", "Next question");
        if (answer_height > answer_view_height()) {
            games_ui_hint_draw(x, 1, "▲▼", "Scroll");
        }
    } else {
        x = games_ui_hint_draw(x, 1, "A", (level == LEVEL_QUESTIONS) ? "Ask" : "Open");
        games_ui_hint_draw(x, 1, "B", "Back");
    }
}

static void draw_answer (void) {
    color_t edge = assistant_color();
    int x0 = LEFT_X - PANEL_PAD, x1 = RIGHT_X1 + PANEL_PAD;
    int y0 = ROWS_Y - PANEL_PAD, y1 = lower_bottom();

    /* Showy: the panel rises into place. */
    float t = animations_progress(answer_since, 220);
    int rise = (t < 1.0f) ? ((int) ((1.0f - animations_ease_out(t)) * 24) & ~1) : 0;
    y0 += rise;

    games_ui_round_fill(x0, y0, x1, y1, edge);
    games_ui_round_fill(x0 + EDGE, y0 + EDGE, x1 - EDGE, y1 - EDGE, theme_get()->panel);

    /* The question, in grey, then the answer under it (scrolled). */
    const question_t *q = &questions[topics[topic].first + question];
    int tx = x0 + EDGE + TEXT_PAD;
    int ty = y0 + EDGE + TEXT_PAD;
    text_at(tx, ty - 4, answer_width() - PERCH, ROW_HEIGHT, ALIGN_LEFT, STL_GRAY, q->text);
    int top = ty + ROW_HEIGHT;
    int bottom = y1 - EDGE - TEXT_PAD;
    if (answer) {
        rdpq_set_scissor(tx, top, tx + answer_width(), bottom);
        rdpq_paragraph_render(answer, tx, top - scroll);
        rdpq_set_scissor(0, 0, display_get_width(), display_get_height());
    }
    if (answer_height > answer_view_height()) {
        /* How far down it is: a thin bar at the right, inside the edge. */
        int track = bottom - top;
        int thumb = (track * track) / answer_height;
        int most = answer_height - answer_view_height();
        int thumb_y = top + ((track - thumb) * (scroll > most ? most : scroll)) / (most > 0 ? most : 1);
        fill(x1 - EDGE - 7, top, x1 - EDGE - 3, bottom, theme_get()->tab_inactive);
        fill(x1 - EDGE - 7, thumb_y, x1 - EDGE - 3, thumb_y + thumb, theme_get()->accent);
    }
}

void assistant_ask_draw (void) {
    const theme_t *th = theme_get();
    const topic_t *t = &topics[topic];
    char title[48];
    snprintf(title, sizeof(title), "Ask %s", assistant_name());

    /* The title, like a heading tab. */
    games_ui_round_fill(LEFT_X, TITLE_Y - 2, LEFT_X + TITLE_WIDTH, TITLE_Y - 2 + ROW_HEIGHT, th->accent);
    text_at(LEFT_X, TITLE_Y - 2, TITLE_WIDTH, ROW_HEIGHT, ALIGN_CENTER, STL_WHITE, title);

    if (level == LEVEL_ANSWER) {
        draw_answer();
    } else {
        int panel_y0 = ROWS_Y - PANEL_PAD, panel_y1 = ROWS_Y + (VISIBLE_ROWS * ROW_HEIGHT) + PANEL_PAD;
        games_ui_round_fill(LEFT_X - PANEL_PAD, panel_y0, LEFT_X + TOPICS_WIDTH + PANEL_PAD, panel_y1, th->panel);
        games_ui_round_fill(QUESTIONS_X - PANEL_PAD, panel_y0, RIGHT_X1 + PANEL_PAD, panel_y1, th->panel);

        static animations_glide_t topic_glide, question_glide;
        selection(LEFT_X, animations_glide(&topic_glide, ROWS_Y + (topic * ROW_HEIGHT)), TOPICS_WIDTH, level == LEVEL_TOPICS);
        for (int i = 0; i < topic_count && i < VISIBLE_ROWS; i++) {
            text_at(LEFT_X + ROW_PADDING, ROWS_Y + (i * ROW_HEIGHT), TOPICS_WIDTH - ROW_PADDING, ROW_HEIGHT, ALIGN_LEFT, STL_DEFAULT, topics[i].name);
        }
        if (level == LEVEL_QUESTIONS) {
            selection(QUESTIONS_X, animations_glide(&question_glide, ROWS_Y + (question * ROW_HEIGHT)), QUESTIONS_WIDTH, true);
        }
        for (int i = 0; i < t->count && i < VISIBLE_ROWS; i++) {
            text_at(QUESTIONS_X + ROW_PADDING, ROWS_Y + (i * ROW_HEIGHT), QUESTIONS_WIDTH - ROW_PADDING, ROW_HEIGHT, ALIGN_LEFT,
                    STL_DEFAULT, questions[t->first + i].text);
        }

        /* Along the bottom: what to do. */
        int x0 = LEFT_X - PANEL_PAD, x1 = RIGHT_X1 + PANEL_PAD;
        games_ui_round_fill(x0, LOWER_Y, x1, lower_bottom(), th->panel);
        char line[160];
        if (level == LEVEL_TOPICS) {
            snprintf(line, sizeof(line), "Pick a topic, then a question, and %s answers. B goes back.", assistant_name());
        } else {
            snprintf(line, sizeof(line), "Press A and %s answers. Left and right in an answer go to the next question.", assistant_name());
        }
        text_at(x0 + TEXT_PAD, LOWER_Y + 4, x1 - x0 - (2 * TEXT_PAD), ROW_HEIGHT - 4, ALIGN_LEFT, STL_DEFAULT, t->name);
        rdpq_text_printf(&(rdpq_textparms_t) { .width = x1 - x0 - (2 * TEXT_PAD), .height = lower_bottom() - LOWER_Y - ROW_HEIGHT - 4,
            .wrap = WRAP_WORD, .style_id = STL_GRAY }, FNT_DEFAULT, x0 + TEXT_PAD, LOWER_Y + ROW_HEIGHT, "%s", line);
    }

    /* The fairy, perched on the top right corner of the panels. */
    assistant_portrait_draw(RIGHT_X1 + PANEL_PAD - (PERCH / 2) - 4, ROWS_Y - PANEL_PAD - (PERCH / 2) + 2, PERCH);

    hints();
}
