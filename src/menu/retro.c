/**
 * @file retro.c
 * @brief The Retro tab: games for the menu's emulators.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libdragon.h>

#include "png_decoder.h"
#include "retro.h"
#include "ui_components/constants.h"
#include "utils/fs.h"

static const struct {
    const char *name, *short_name, *kind, *folder;
    float aspect;
} systems[RETRO_SYSTEMS] = {
    [RETRO_OTHER] = { "Other", "EMU", "Game for an emulator", NULL, 1.4f },
    [RETRO_NES] = { "NES", "NES", "NES game", "nes", 5.0f / 7.0f },          /* the American box, tall */
    [RETRO_SNES] = { "SNES", "SNES", "SNES game", "snes", 7.5f / 5.5f },     /* the American box, wide */
    [RETRO_GB] = { "Game Boy", "GB", "Game Boy game", "gb", 0.95f },          /* nearly square */
    [RETRO_GBC] = { "Game Boy Color", "GBC", "Game Boy Color game", "gbc", 0.95f },
};

retro_system_t retro_system (const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) {
        return RETRO_OTHER;
    }
    dot++;
    if (!strcasecmp(dot, "nes")) return RETRO_NES;
    if (!strcasecmp(dot, "sfc") || !strcasecmp(dot, "smc")) return RETRO_SNES;
    if (!strcasecmp(dot, "gb")) return RETRO_GB;
    if (!strcasecmp(dot, "gbc")) return RETRO_GBC;
    return RETRO_OTHER;
}

static retro_system_t valid (retro_system_t s) {
    return (s >= 0 && s < RETRO_SYSTEMS) ? s : RETRO_OTHER;
}

const char *retro_name (retro_system_t system) {
    return systems[valid(system)].name;
}

const char *retro_short_name (retro_system_t system) {
    return systems[valid(system)].short_name;
}

const char *retro_kind (retro_system_t system) {
    return systems[valid(system)].kind;
}

const char *retro_folder (retro_system_t system) {
    return systems[valid(system)].folder;
}

float retro_aspect (retro_system_t system) {
    return systems[valid(system)].aspect;
}

static void decoded (png_err_t err, surface_t *image, void *data) {
    component_boxart_t *b = data;
    b->loading = false;
    b->image = image;
}

component_boxart_t *retro_art_load (const char *storage_prefix, const char *name, file_image_type_t which) {
    retro_system_t system = retro_system(name);
    if (!systems[system].folder) {
        return NULL;
    }
    /* The file name without its folder and extension. */
    const char *base = strrchr(name, '/');
    base = base ? base + 1 : name;
    char title[200];
    snprintf(title, sizeof(title), "%s", base);
    char *dot = strrchr(title, '.');
    if (dot) {
        *dot = '\0';
    }
    char folder[260];
    snprintf(folder, sizeof(folder), "menu/metadata/%s/%s", systems[system].folder, title);
    path_t *path = path_init(storage_prefix, folder);
    const char *file;
    switch (which) {
        case IMAGE_BOXART_BACK: file = "boxart_back.png"; break;
        case IMAGE_BOXART_TOP: file = "boxart_top.png"; break;
        case IMAGE_BOXART_LEFT:
        case IMAGE_BOXART_RIGHT: file = "boxart_spine.png"; break;     /* one spine, on whichever side shows */
        case IMAGE_GAMEPAK_FRONT: file = "gamepak_front.png"; break;
        case IMAGE_GAMEPAK_BACK: file = "gamepak_back.png"; break;
        default: file = "boxart_front.png"; break;
    }
    path_push(path, (char *) file);
    component_boxart_t *b = NULL;
    if (file_exists(path_get(path)) && (b = calloc(1, sizeof(component_boxart_t)))) {
        b->loading = true;
        if (png_decoder_start(path_get(path), BOXART_WIDTH_MAX, BOXART_HEIGHT_MAX, decoded, b) != PNG_OK) {
            free(b);
            b = NULL;
        }
    }
    debugf("retro: art %s %s\n", path_get(path), b ? "found" : "not found");
    path_free(path);
    return b;
}
