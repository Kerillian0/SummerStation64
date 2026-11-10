/* PC-side tests for the theme.ini reader (src/menu/theme_parse.c).
   Run with tests/run.sh. */

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "theme_parse.h"

/* The real table lives in menu_features.c, which needs the console's
   libraries; two keys are enough to check that [features] is read. */
feature_t feature_from_key (const char *key) {
    if (!strcasecmp(key, "quick_launch")) return FEATURE_QUICK_LAUNCH;
    if (!strcasecmp(key, "side_covers")) return FEATURE_SIDE_COVERS;
    return FEATURE_COUNT;
}

const char *feature_key (feature_t feature) {
    return (feature == FEATURE_QUICK_LAUNCH) ? "quick_launch" : (feature == FEATURE_SIDE_COVERS) ? "side_covers" : "unknown";
}

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static bool same (color_t c, int r, int g, int b) {
    return c.r == r && c.g == g && c.b == b && c.a == 0xFF;
}

static const char *write_file (const char *text) {
    static char path[] = "/tmp/theme_parse_test.ini";
    FILE *f = fopen(path, "w");
    fputs(text, f);
    fclose(f);
    return path;
}

/* A theme with known values in every field, standing in for the defaults. */
static void start (theme_t *t) {
    memset(t, 0, sizeof(*t));
    strcpy(t->name, "Default");
    t->text = RGBA32(1, 2, 3, 0xFF);
    t->accent = RGBA32(4, 5, 6, 0xFF);
    t->color1 = RGBA32(7, 8, 9, 0xFF);
    t->bg_type = THEME_BG_GRADIENT;
    t->direction = THEME_DIR_VERTICAL;
    t->dither = true;
    t->pattern_size = 16;
    t->pattern_opacity = 10;
    for (int i = 0; i < FEATURE_COUNT; i++) t->features[i] = FEATURE_UNSET;
}

static void test_full_file (void) {
    printf("A complete theme.ini, as the Theme Maker writes it\n");
    theme_t t;
    start(&t);
    CHECK(theme_parse_file(&t, write_file(
        "[theme]\n"
        "name = Sunset Drive\n"
        "author = Kerillian\n"
        "\n"
        "[colors]\n"
        "text = FFF4EC\n"
        "text_dim = #EBCFC4\n"
        "accent = ffd166\n"
        "panel = 2A1730\n"
        "border = FFD166\n"
        "highlight = 8A3A5E\n"
        "tab_active = A8406A\n"
        "tab_inactive = 3E1F4F\n"
        "tab_active_border = FFD166\n"
        "tab_inactive_border = 6B3A6E\n"
        "\n"
        "[background]\n"
        "type = gradient\n"
        "color1 = 3E1F4F\n"
        "color2 = E0703A\n"
        "color3 = A8406A\n"
        "direction = diagonal\n"
        "dither = 0\n"
        "\n"
        "[pattern]\n"
        "style = scanlines\n"
        "color = 000000\n"
        "size = 8\n"
        "opacity = 18\n"
        "\n"
        "[features]\n"
        "quick_launch = 1\n"
        "side_covers = 0\n")));
    CHECK(!strcmp(t.name, "Sunset Drive"));
    CHECK(!strcmp(t.author, "Kerillian"));
    CHECK(same(t.text, 0xFF, 0xF4, 0xEC));
    CHECK(same(t.text_dim, 0xEB, 0xCF, 0xC4));     /* with a leading # */
    CHECK(same(t.accent, 0xFF, 0xD1, 0x66));       /* lower case */
    CHECK(same(t.panel, 0x2A, 0x17, 0x30));
    CHECK(same(t.border, 0xFF, 0xD1, 0x66));
    CHECK(same(t.highlight, 0x8A, 0x3A, 0x5E));
    CHECK(same(t.tab_active, 0xA8, 0x40, 0x6A));
    CHECK(same(t.tab_inactive, 0x3E, 0x1F, 0x4F));
    CHECK(same(t.tab_active_border, 0xFF, 0xD1, 0x66));
    CHECK(same(t.tab_inactive_border, 0x6B, 0x3A, 0x6E));
    CHECK(t.bg_type == THEME_BG_GRADIENT);
    CHECK(same(t.color1, 0x3E, 0x1F, 0x4F));
    CHECK(same(t.color2, 0xE0, 0x70, 0x3A));
    CHECK(t.use_color3 && same(t.color3, 0xA8, 0x40, 0x6A));
    CHECK(t.direction == THEME_DIR_DIAGONAL);
    CHECK(!t.dither);
    CHECK(t.pattern == THEME_PATTERN_SCANLINES);
    CHECK(same(t.pattern_color, 0, 0, 0));
    CHECK(t.pattern_size == 8);
    CHECK(t.pattern_opacity == 18);
    CHECK(t.features[FEATURE_QUICK_LAUNCH] == 1);
    CHECK(t.features[FEATURE_SIDE_COVERS] == 0);
    CHECK(t.features[FEATURE_CAROUSEL_ANIMATION] == FEATURE_UNSET);
}

static void test_bad_values (void) {
    printf("Bad values leave the defaults alone\n");
    theme_t t;
    start(&t);
    CHECK(theme_parse_file(&t, write_file(
        "[colors]\n"
        "text = FFF\n"            /* too short */
        "accent = GG0000\n"       /* not hex */
        "panel = 1234567\n"       /* too long */
        "[background]\n"
        "type = video\n"          /* unknown */
        "direction = sideways\n"
        "color3 = none\n"         /* the documented way to switch it off */
        "[pattern]\n"
        "size = lots\n"
        "[features]\n"
        "no_such_feature = 1\n")));
    CHECK(same(t.text, 1, 2, 3));
    CHECK(same(t.accent, 4, 5, 6));
    CHECK(t.panel.r == 0 && t.panel.a == 0);       /* untouched (zero in start()) */
    CHECK(t.bg_type == THEME_BG_GRADIENT);
    CHECK(t.direction == THEME_DIR_VERTICAL);
    CHECK(!t.use_color3);
    CHECK(t.pattern_size == 16);
}

static void test_limits (void) {
    printf("Numbers are held inside their limits\n");
    theme_t t;
    start(&t);
    CHECK(theme_parse_file(&t, write_file(
        "[pattern]\nsize = 1000\nopacity = -5\n")));
    CHECK(t.pattern_size == 128);
    CHECK(t.pattern_opacity == 0);
    start(&t);
    CHECK(theme_parse_file(&t, write_file(
        "[pattern]\nsize = 0\nopacity = 250\n")));
    CHECK(t.pattern_size == 2);
    CHECK(t.pattern_opacity == 100);
}

static void test_layout (void) {
    printf("Comments, blank lines, spacing, case and Windows line endings\n");
    theme_t t;
    start(&t);
    CHECK(theme_parse_file(&t, write_file(
        "; made by hand\r\n"
        "# another comment\r\n"
        "\r\n"
        "  [ Background ]  \r\n"
        "TYPE=ocean\r\n"
        "   color1   =   102030   \r\n"
        "[Theme]\r\n"
        "Name = A name that is far too long to fit in the forty characters kept for it\r\n"
        "line without an equals sign\r\n")));
    CHECK(t.bg_type == THEME_BG_OCEAN);
    CHECK(same(t.color1, 0x10, 0x20, 0x30));
    CHECK(strlen(t.name) == sizeof(t.name) - 1);   /* cut, not overflowing */
    CHECK(!strncmp(t.name, "A name that is far too long", 27));
}

static void test_keys_outside_their_section (void) {
    printf("A key only counts in its own section\n");
    theme_t t;
    start(&t);
    CHECK(theme_parse_file(&t, write_file(
        "text = FFFFFF\n"             /* before any section */
        "[pattern]\n"
        "accent = FFFFFF\n")));       /* wrong section */
    CHECK(same(t.text, 1, 2, 3));
    CHECK(same(t.accent, 4, 5, 6));
}

static void test_missing_file (void) {
    printf("A missing file is reported and changes nothing\n");
    theme_t t;
    start(&t);
    CHECK(!theme_parse_file(&t, "/tmp/no/such/theme.ini"));
    CHECK(!strcmp(t.name, "Default"));
}

static void test_write_read_back (void) {
    printf("A theme written by the editor reads back the same, over other defaults\n");
    theme_t a;
    start(&a);
    strcpy(a.name, "Lava Lamp");
    a.text = RGBA32(0xFF, 0xF4, 0xEC, 0xFF);
    a.text_dim = RGBA32(0xC0, 0xA0, 0x90, 0xFF);
    a.accent = RGBA32(0xFF, 0x60, 0x20, 0xFF);
    a.panel = RGBA32(0x30, 0x10, 0x10, 0xFF);
    a.border = RGBA32(0x11, 0x22, 0x33, 0xFF);
    a.highlight = RGBA32(0x44, 0x55, 0x66, 0xFF);
    a.tab_active = RGBA32(0x77, 0x88, 0x99, 0xFF);
    a.tab_inactive = RGBA32(0xAA, 0xBB, 0xCC, 0xFF);
    a.tab_active_border = RGBA32(0xDD, 0xEE, 0xFF, 0xFF);
    a.tab_inactive_border = RGBA32(0x01, 0x02, 0x03, 0xFF);
    a.bg_type = THEME_BG_OCEAN;
    a.direction = THEME_DIR_RADIAL;
    a.color1 = RGBA32(0x10, 0x20, 0x30, 0xFF);
    a.color2 = RGBA32(0x40, 0x50, 0x60, 0xFF);
    a.use_color3 = false;
    a.dither = false;
    a.pattern = THEME_PATTERN_SCANLINES;
    a.pattern_color = RGBA32(0x70, 0x80, 0x90, 0xFF);
    a.pattern_size = 64;
    a.pattern_opacity = 35;
    a.features[FEATURE_QUICK_LAUNCH] = 1;
    a.features[FEATURE_SIDE_COVERS] = 0;
    CHECK(theme_write_file(&a, "/tmp/theme_write_test.ini"));

    /* Read over a theme whose every field differs, as Midnight's would. */
    theme_t b;
    start(&b);
    b.use_color3 = true;
    b.color3 = RGBA32(9, 9, 9, 0xFF);
    b.dither = true;
    b.features[FEATURE_CAROUSEL_ANIMATION] = 1;
    CHECK(theme_parse_file(&b, "/tmp/theme_write_test.ini"));
    CHECK(!strcmp(b.name, "Lava Lamp"));
    CHECK(same(b.text, 0xFF, 0xF4, 0xEC));
    CHECK(same(b.text_dim, 0xC0, 0xA0, 0x90));
    CHECK(same(b.accent, 0xFF, 0x60, 0x20));
    CHECK(same(b.panel, 0x30, 0x10, 0x10));
    CHECK(same(b.border, 0x11, 0x22, 0x33));
    CHECK(same(b.highlight, 0x44, 0x55, 0x66));
    CHECK(same(b.tab_active, 0x77, 0x88, 0x99));
    CHECK(same(b.tab_inactive, 0xAA, 0xBB, 0xCC));
    CHECK(same(b.tab_active_border, 0xDD, 0xEE, 0xFF));
    CHECK(same(b.tab_inactive_border, 0x01, 0x02, 0x03));
    CHECK(b.bg_type == THEME_BG_OCEAN);
    CHECK(b.direction == THEME_DIR_RADIAL);
    CHECK(same(b.color1, 0x10, 0x20, 0x30));
    CHECK(same(b.color2, 0x40, 0x50, 0x60));
    CHECK(!b.use_color3);                      /* "color3 = none" turns the middle color off */
    CHECK(!b.dither);
    CHECK(b.pattern == THEME_PATTERN_SCANLINES);
    CHECK(same(b.pattern_color, 0x70, 0x80, 0x90));
    CHECK(b.pattern_size == 64);
    CHECK(b.pattern_opacity == 35);
    CHECK(b.features[FEATURE_QUICK_LAUNCH] == 1);
    CHECK(b.features[FEATURE_SIDE_COVERS] == 0);
    CHECK(b.features[FEATURE_CAROUSEL_ANIMATION] == 1);    /* not in the file: left as it was */

    printf("The middle color and the other choices, written and read back\n");
    a.bg_type = THEME_BG_GRADIENT;
    a.direction = THEME_DIR_DIAGONAL;
    a.use_color3 = true;
    a.color3 = RGBA32(0xAB, 0xCD, 0xEF, 0xFF);
    a.dither = true;
    a.pattern = THEME_PATTERN_NONE;
    CHECK(theme_write_file(&a, "/tmp/theme_write_test.ini"));
    start(&b);
    CHECK(theme_parse_file(&b, "/tmp/theme_write_test.ini"));
    CHECK(b.bg_type == THEME_BG_GRADIENT);
    CHECK(b.direction == THEME_DIR_DIAGONAL);
    CHECK(b.use_color3 && same(b.color3, 0xAB, 0xCD, 0xEF));
    CHECK(b.dither);
    CHECK(b.pattern == THEME_PATTERN_NONE);
    for (int d = THEME_DIR_VERTICAL; d <= THEME_DIR_RADIAL; d++) {
        a.direction = (theme_dir_t) d;
        theme_write_file(&a, "/tmp/theme_write_test.ini");
        start(&b);
        b.direction = (d == THEME_DIR_VERTICAL) ? THEME_DIR_RADIAL : THEME_DIR_VERTICAL;
        theme_parse_file(&b, "/tmp/theme_write_test.ini");
        CHECK(b.direction == (theme_dir_t) d);
    }
    for (int p = THEME_PATTERN_NONE; p <= THEME_PATTERN_SCANLINES; p++) {
        a.pattern = (theme_pattern_t) p;
        theme_write_file(&a, "/tmp/theme_write_test.ini");
        start(&b);
        b.pattern = (p == THEME_PATTERN_NONE) ? THEME_PATTERN_GRID : THEME_PATTERN_NONE;
        theme_parse_file(&b, "/tmp/theme_write_test.ini");
        CHECK(b.pattern == (theme_pattern_t) p);
    }
    for (int bg = THEME_BG_SOLID; bg <= THEME_BG_OCEAN; bg++) {
        a.bg_type = (theme_bg_type_t) bg;
        theme_write_file(&a, "/tmp/theme_write_test.ini");
        start(&b);
        b.bg_type = (bg == THEME_BG_SOLID) ? THEME_BG_OCEAN : THEME_BG_SOLID;
        theme_parse_file(&b, "/tmp/theme_write_test.ini");
        CHECK(b.bg_type == (theme_bg_type_t) bg);
    }
}

int main (void) {
    test_full_file();
    test_bad_values();
    test_limits();
    test_layout();
    test_keys_outside_their_section();
    test_missing_file();
    test_write_read_back();
    printf("theme parser: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
