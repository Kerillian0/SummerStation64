#include <stdarg.h>
#include "../bookkeeping.h"
#include "../fonts.h"
#include "../ui_components/constants.h"
#include "../sound.h"
#include "views.h"
#include "../controls.h"
#include "../display_name.h"
#include "../games_ui.h"
#include "../cover_row.h"
#include "../tabs.h"
#include "../start_menu.h"


typedef enum {
    BOOKKEEPING_TAB_CONTEXT_HISTORY,
    BOOKKEEPING_TAB_CONTEXT_FAVORITE,
    BOOKKEEPING_TAB_CONTEXT_NONE
} bookkeeping_tab_context_t;


// Both tabs are rows of covers now. 0 brings the stock text lists back.
#define COVER_ROWS  (1)

static bool confirm_remove = false;  // "Remove this favorite?" is showing

static bookkeeping_tab_context_t tab_context = BOOKKEEPING_TAB_CONTEXT_NONE;
static int selected_item = -1;
static bookkeeping_item_t *item_list;
static uint16_t item_max = 0;


static void item_reset_selected(menu_t *menu) {
    selected_item = -1;

    for(uint16_t i=0; i<item_max; i++) {
        if(item_list[i].bookkeeping_type != BOOKKEEPING_TYPE_EMPTY) {
            selected_item = i;
            break;
        }
    }  
}

static void item_move_next() {
    int last = selected_item;

    do
    {
        selected_item++;

        if(selected_item >= item_max) {
            selected_item = last;
            break;
        } else if(item_list[selected_item].bookkeeping_type != BOOKKEEPING_TYPE_EMPTY) {
            sound_play_effect(SFX_CURSOR);
            break;
        }
    } while (true);  
}

static void item_move_previous() {
    int last = selected_item;
    do
    {
        selected_item--;

        if(selected_item < 0) {
            selected_item = last;
            break;
        } else if(item_list[selected_item].bookkeeping_type != BOOKKEEPING_TYPE_EMPTY) {
            sound_play_effect(SFX_CURSOR);
            break;
        }
    } while (true);
}

static void process(menu_t *menu) {
    if (start_menu_process(menu)) {
        return; // the START menu is open
    }

    // Z asks first: it is easy to press by accident.
    if (confirm_remove) {
        if (menu->actions.enter && selected_item != -1) {
            confirm_remove = false;
            bookkeeping_favorite_remove(&menu->bookkeeping, selected_item);
            item_reset_selected(menu);
            if (COVER_ROWS) cover_row_open(menu, item_list, item_max); // the list changed: build the row again
            sound_play_effect(SFX_SETTING);
        } else if (menu->actions.back || menu->actions.options) {
            confirm_remove = false;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }
    if (COVER_ROWS) {
        // A row of covers: left/right move along it, L/R switch tabs.
        controls_remap_tabs(menu, true);
        selected_item = cover_row_process(menu);
        menu->actions.go_up = false;
        menu->actions.go_down = false;
    } else {
        controls_remap_tabs(menu, false); // L/R tabs, Z options
    }
    if(menu->actions.go_down) {
        item_move_next();   
    } else if(menu->actions.go_up) {
        item_move_previous();
    } else if(menu->actions.enter && selected_item != -1) {
                
        if(tab_context == BOOKKEEPING_TAB_CONTEXT_FAVORITE) {
            menu->load.load_favorite_id = selected_item;
            menu->load.load_history_id = -1;
        } else if(tab_context == BOOKKEEPING_TAB_CONTEXT_HISTORY) {
            menu->load.load_history_id = selected_item;
            menu->load.load_favorite_id = -1;
        }           

        if(item_list[selected_item].bookkeeping_type == BOOKKEEPING_TYPE_DISK) {
            menu->next_mode = MENU_MODE_LOAD_DISK;
            sound_play_effect(SFX_ENTER);
        } else if(item_list[selected_item].bookkeeping_type == BOOKKEEPING_TYPE_ROM) {
            menu->next_mode = MENU_MODE_LOAD_ROM;
            sound_play_effect(SFX_ENTER);
        }
    } else if (menu->actions.go_left || menu->actions.go_right) {
        // L and R: the tab before or after this one, in the player's order.
        games_tab_t here = (tab_context == BOOKKEEPING_TAB_CONTEXT_FAVORITE) ? GAMES_TAB_FAVORITES : GAMES_TAB_RECENT;
        tabs_open(menu, tabs_step(here, menu->actions.go_right ? 1 : -1));
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.settings) {
        start_menu_show();
    }else if(tab_context == BOOKKEEPING_TAB_CONTEXT_FAVORITE && menu->actions.options && selected_item != -1) {
        confirm_remove = true;
        sound_play_effect(SFX_SETTING);
    }
}

static void draw_list(menu_t *menu, surface_t *display) {
    if(selected_item != -1) {
        float highlight_y = GAMES_UI_LIST_TOP + (selected_item * 19 * 2);

        ui_components_box_draw(
            VISIBLE_AREA_X0,
            highlight_y,
            VISIBLE_AREA_X0 + FILE_LIST_HIGHLIGHT_WIDTH + LIST_SCROLLBAR_WIDTH,
            highlight_y + 39,
            FILE_LIST_HIGHLIGHT_COLOR
        );
    }

    char buffer[1024];
    buffer[0] = 0;

    for(uint16_t i=0; i < item_max; i++) {   
        if(path_has_value(item_list[i].primary_path)) {
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%d  : %s\n", (i+1), display_name_file(path_last_get(item_list[i].primary_path)));
        } else {
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%d  : \n", (i+1));
        }

        if(path_has_value(item_list[i].secondary_path)) {
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "     %s\n", display_name_file(path_last_get(item_list[i].secondary_path)));
        } else {
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\n");
        }
    }

    int nbytes = strlen(buffer);
    rdpq_text_printn(
        &(rdpq_textparms_t) {
            .width = VISIBLE_AREA_WIDTH - (TEXT_MARGIN_HORIZONTAL * 2),
            .height = LAYOUT_ACTIONS_SEPARATOR_Y - OVERSCAN_HEIGHT - (TEXT_MARGIN_VERTICAL * 2),
            .align = ALIGN_LEFT,
            .valign = VALIGN_TOP,
            .wrap = WRAP_ELLIPSES,
            .line_spacing = TEXT_OFFSET_VERTICAL,
        },
        FNT_DEFAULT,
        VISIBLE_AREA_X0 + TEXT_MARGIN_HORIZONTAL,
        GAMES_UI_LIST_TOP,
        buffer,
        nbytes
    );           
}

static void draw(menu_t *menu, surface_t *display) {
    rdpq_attach(display, NULL);

    ui_components_background_draw();

    if(tab_context == BOOKKEEPING_TAB_CONTEXT_FAVORITE) {
        games_ui_topbar_draw(menu, GAMES_TAB_FAVORITES);
    } else if(tab_context == BOOKKEEPING_TAB_CONTEXT_HISTORY) {
        games_ui_topbar_draw(menu, GAMES_TAB_RECENT);
    }

    // (the redesigned screens have no frame)

    if (COVER_ROWS) {
        if (tab_context == BOOKKEEPING_TAB_CONTEXT_FAVORITE) {
            cover_row_draw(menu, "No favorites yet\n\nAdd one from a game's info screen: Options", "Remove");
        } else {
            cover_row_draw(menu, "No games played yet", NULL);
        }
        games_ui_hint_right_draw(0, "START", "Settings");
        start_menu_draw();
        if (confirm_remove && selected_item != -1) {
            ui_components_messagebox_draw(
                "Remove from Favorites?\n\n"
                "%s\n\n"
                "A: Remove    B: Keep",
                display_name_file(path_last_get(item_list[selected_item].primary_path))
            );
        }
        rdpq_detach_show();
        return;
    }

    draw_list(menu, display);

    if(selected_item != -1) {
        ui_components_actions_bar_text_draw(
            STL_DEFAULT,
            ALIGN_LEFT, VALIGN_TOP,
            "%s\n"
            "\n",
            controls_rom_hint()
        );
        
        if(tab_context == BOOKKEEPING_TAB_CONTEXT_FAVORITE && selected_item != -1) {
            ui_components_actions_bar_text_draw(
                STL_DEFAULT,
                ALIGN_RIGHT, VALIGN_TOP,
                "Z: Remove item\n"
                "\n"
            );
        }
    }

    ui_components_actions_bar_text_draw(
        STL_DEFAULT,
        ALIGN_CENTER, VALIGN_TOP,
        "START: Settings\n"
        "\n"
    );    

    start_menu_draw();

    rdpq_detach_show();   
}

void view_favorite_init (menu_t *menu) {
    tab_context = BOOKKEEPING_TAB_CONTEXT_FAVORITE;
    confirm_remove = false;
    start_menu_init();
    games_ui_set_origin(MENU_MODE_FAVORITE);
    item_list = menu->bookkeeping.favorite_items;
    item_max = FAVORITES_COUNT;

    item_reset_selected(menu);
    if (COVER_ROWS) cover_row_open(menu, item_list, item_max);
}

void view_favorite_display (menu_t *menu, surface_t *display) {
    process(menu);
    draw(menu, display); 
    if (menu->next_mode != MENU_MODE_FAVORITE) cover_row_close(); // free the covers before another screen loads its own
}

void view_history_init (menu_t *menu) {
    tab_context = BOOKKEEPING_TAB_CONTEXT_HISTORY;
    start_menu_init();
    games_ui_set_origin(MENU_MODE_HISTORY);
    item_list = menu->bookkeeping.history_items;
    item_max = HISTORY_COUNT;

    item_reset_selected(menu);
    if (COVER_ROWS) cover_row_open(menu, item_list, item_max);
}

void view_history_display (menu_t *menu, surface_t *display) {
    process(menu);
    draw(menu, display); 
    if (menu->next_mode != MENU_MODE_HISTORY) cover_row_close(); // free the covers before another screen loads its own
}
