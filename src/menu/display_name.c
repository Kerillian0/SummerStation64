#include <string.h>
#include <strings.h>

#include "display_name.h"
#include "menu_features.h"

#define NAME_LENGTH (256)

static char shown[NAME_LENGTH];

static bool is_game (const entry_t *entry) {
    return entry->type == ENTRY_TYPE_ROM || entry->type == ENTRY_TYPE_DISK || entry->type == ENTRY_TYPE_EMULATOR;
}

/* "Name, The - Subtitle (U)" -> "The Name - Subtitle (U)". ROM sets put the
   article last so the files sort well; people read it the other way round. */
static void article_to_front (char *name) {
    static const char *const articles[] = { "The", "An", "A" };

    for (int i = 0; i < (int) (sizeof(articles) / sizeof(articles[0])); i++) {
        size_t article_length = strlen(articles[i]);

        for (char *comma = strchr(name, ','); comma; comma = strchr(comma + 1, ',')) {
            if (comma[1] != ' ' || strncasecmp(comma + 2, articles[i], article_length) != 0) {
                continue;
            }

            /* The article must be a whole word that ends this part of the title. */
            const char *after = comma + 2 + article_length;
            bool ends_title = (after[0] == '\0') ||
                (after[0] == ' ' && (after[1] == '-' || after[1] == '(' || after[1] == '['));
            if (!ends_title) {
                continue;
            }

            char moved[NAME_LENGTH];
            size_t front_length = comma - name;
            if (article_length + 1 + front_length + strlen(after) >= sizeof(moved)) {
                return;
            }
            memcpy(moved, comma + 2, article_length);       /* keep the file's own spelling */
            moved[article_length] = ' ';
            memcpy(moved + article_length + 1, name, front_length);
            strcpy(moved + article_length + 1 + front_length, after);
            strcpy(name, moved);
            return;
        }
    }
}

/* "Name (U) (V1.2) [!]" -> "Name": drops every (...) and [...] group. */
static void strip_tags (char *name) {
    char kept[NAME_LENGTH];
    size_t length = 0;

    for (const char *c = name; *c; c++) {
        if (*c == '(' || *c == '[') {
            const char *close = strchr(c, (*c == '(') ? ')' : ']');
            if (close) {
                c = close; /* skip the whole group */
                continue;
            }
        }
        /* Don't leave two spaces where a group was cut out. */
        if (*c == ' ' && (length == 0 || kept[length - 1] == ' ')) {
            continue;
        }
        kept[length++] = *c;
    }
    while (length > 0 && kept[length - 1] == ' ') {
        length--;
    }
    kept[length] = '\0';

    /* A name that was nothing but tags stays as it is. */
    if (length > 0) {
        strcpy(name, kept);
    }
}

const char *display_name (const entry_t *entry) {
    if (!entry || !entry->name) {
        return "";
    }
    if (!is_game(entry)) {
        return entry->name;
    }

    bool hide_extension = features_enabled(FEATURE_HIDE_EXTENSIONS);
    bool tidy = features_enabled(FEATURE_TIDY_TITLES);
    bool hide_tags = features_enabled(FEATURE_HIDE_TAGS);
    if (!hide_extension && !tidy && !hide_tags) {
        return entry->name;
    }

    strncpy(shown, entry->name, sizeof(shown) - 1);
    shown[sizeof(shown) - 1] = '\0';

    /* Set the extension aside while the title is tidied. */
    char extension[16] = "";
    char *dot = strrchr(shown, '.');
    if (dot && dot != shown && strlen(dot) < sizeof(extension)) {
        strcpy(extension, dot);
        *dot = '\0';
    }

    if (hide_tags) {
        strip_tags(shown);
    }

    if (tidy) {
        article_to_front(shown);
    }

    if (!hide_extension && (strlen(shown) + strlen(extension) < sizeof(shown))) {
        strcat(shown, extension);
    }

    return shown;
}
