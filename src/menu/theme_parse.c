/**
 * @file theme_parse.c
 * @brief Reads a theme.ini file into a theme.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "theme_parse.h"

static char *trim (char *s) {
    while (isspace((unsigned char) *s)) {
        s++;
    }
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char) end[-1])) {
        end--;
    }
    *end = '\0';
    return s;
}

static bool parse_hex_color (const char *v, color_t *out) {
    if (*v == '#') {
        v++;
    }
    if (strlen(v) != 6) {
        return false;
    }
    char *end;
    unsigned long n = strtoul(v, &end, 16);
    if (*end != '\0') {
        return false;
    }
    *out = RGBA32((n >> 16) & 0xFF, (n >> 8) & 0xFF, n & 0xFF, 0xFF);
    return true;
}

static void parse_int (const char *v, int *out, int min, int max) {
    char *end;
    long n = strtol(v, &end, 10);
    if (end != v) {
        if (n < min) n = min;
        if (n > max) n = max;
        *out = (int) n;
    }
}

static void copy_str (char *dst, size_t size, const char *src) {
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

static void apply_key (theme_t *t, const char *section, const char *key, const char *v) {
    if (!strcasecmp(section, "theme")) {
        if (!strcasecmp(key, "name")) copy_str(t->name, sizeof(t->name), v);
        else if (!strcasecmp(key, "author")) copy_str(t->author, sizeof(t->author), v);
    } else if (!strcasecmp(section, "colors")) {
        if (!strcasecmp(key, "text")) parse_hex_color(v, &t->text);
        else if (!strcasecmp(key, "text_dim")) parse_hex_color(v, &t->text_dim);
        else if (!strcasecmp(key, "accent")) parse_hex_color(v, &t->accent);
        else if (!strcasecmp(key, "panel")) parse_hex_color(v, &t->panel);
        else if (!strcasecmp(key, "border")) parse_hex_color(v, &t->border);
        else if (!strcasecmp(key, "highlight")) parse_hex_color(v, &t->highlight);
        else if (!strcasecmp(key, "tab_active")) parse_hex_color(v, &t->tab_active);
        else if (!strcasecmp(key, "tab_inactive")) parse_hex_color(v, &t->tab_inactive);
        else if (!strcasecmp(key, "tab_active_border")) parse_hex_color(v, &t->tab_active_border);
        else if (!strcasecmp(key, "tab_inactive_border")) parse_hex_color(v, &t->tab_inactive_border);
    } else if (!strcasecmp(section, "background")) {
        if (!strcasecmp(key, "type")) {
            if (!strcasecmp(v, "solid")) t->bg_type = THEME_BG_SOLID;
            else if (!strcasecmp(v, "gradient")) t->bg_type = THEME_BG_GRADIENT;
            else if (!strcasecmp(v, "image")) t->bg_type = THEME_BG_IMAGE;
            else if (!strcasecmp(v, "ocean")) t->bg_type = THEME_BG_OCEAN;
        } else if (!strcasecmp(key, "direction")) {
            if (!strcasecmp(v, "vertical")) t->direction = THEME_DIR_VERTICAL;
            else if (!strcasecmp(v, "horizontal")) t->direction = THEME_DIR_HORIZONTAL;
            else if (!strcasecmp(v, "diagonal")) t->direction = THEME_DIR_DIAGONAL;
            else if (!strcasecmp(v, "radial")) t->direction = THEME_DIR_RADIAL;
        } else if (!strcasecmp(key, "color1")) {
            parse_hex_color(v, &t->color1);
        } else if (!strcasecmp(key, "color2")) {
            parse_hex_color(v, &t->color2);
        } else if (!strcasecmp(key, "color3")) {
            t->use_color3 = parse_hex_color(v, &t->color3);
        } else if (!strcasecmp(key, "dither")) {
            t->dither = (atoi(v) != 0);
        } else if (!strcasecmp(key, "image")) {
            copy_str(t->image, sizeof(t->image), v);
        }
    } else if (!strcasecmp(section, "features")) {
        feature_t f = feature_from_key(key);
        if (f < FEATURE_COUNT) {
            t->features[f] = (atoi(v) != 0) ? 1 : 0;
        }
    } else if (!strcasecmp(section, "pattern")) {
        if (!strcasecmp(key, "style")) {
            if (!strcasecmp(v, "none")) t->pattern = THEME_PATTERN_NONE;
            else if (!strcasecmp(v, "stripes")) t->pattern = THEME_PATTERN_STRIPES;
            else if (!strcasecmp(v, "diagonal")) t->pattern = THEME_PATTERN_DIAGONAL;
            else if (!strcasecmp(v, "checker")) t->pattern = THEME_PATTERN_CHECKER;
            else if (!strcasecmp(v, "dots")) t->pattern = THEME_PATTERN_DOTS;
            else if (!strcasecmp(v, "grid")) t->pattern = THEME_PATTERN_GRID;
            else if (!strcasecmp(v, "scanlines")) t->pattern = THEME_PATTERN_SCANLINES;
        } else if (!strcasecmp(key, "color")) {
            parse_hex_color(v, &t->pattern_color);
        } else if (!strcasecmp(key, "size")) {
            parse_int(v, &t->pattern_size, 2, 128);
        } else if (!strcasecmp(key, "opacity")) {
            parse_int(v, &t->pattern_opacity, 0, 100);
        }
    }
}

bool theme_parse_file (theme_t *t, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return false;
    }

    char line[160];
    char section[32] = "";

    while (fgets(line, sizeof(line), f)) {
        char *s = trim(line);
        if (*s == '\0' || *s == ';' || *s == '#') {
            continue;
        }
        if (*s == '[') {
            char *end = strchr(s, ']');
            if (end) {
                *end = '\0';
                copy_str(section, sizeof(section), trim(s + 1));
            }
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        apply_key(t, section, trim(s), trim(eq + 1));
    }

    fclose(f);
    return true;
}
