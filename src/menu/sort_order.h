/**
 * @file sort_order.h
 * @brief Ways to order the file list.
 */

#ifndef SORT_ORDER_H__
#define SORT_ORDER_H__

#include "menu_state.h"

typedef enum {
    SORT_BY_TYPE,       /**< the stock order: folders, then each file type, by name */
    SORT_NAME_AZ,       /**< folders first, then everything by name */
    SORT_NAME_ZA,       /**< folders first, then everything by name, reversed */
    SORT_RECENT_FIRST,  /**< recently played games first, then as SORT_NAME_AZ */
    SORT_RELEASE_DATE,  /**< games by release date, oldest first; unknown dates last, then as SORT_NAME_AZ */
    SORT_DEVELOPER,     /**< games by who made them, each developer's by date; unknown last */
    SORT_COUNT
} sort_order_t;

/** Label for the Settings screen. */
const char *sort_order_name (int order);

/**
 * Sort the browser's file list by the player's chosen order.
 * @param stock_compare The browser's own comparison, used for SORT_BY_TYPE.
 */
void sort_order_apply (menu_t *menu, int (*stock_compare) (const void *, const void *));

/**
 * What the chosen order goes by for this entry, to show beside it: the
 * developer's name, or "Released 1998". NULL when the order is by name or
 * type, or nothing is known about the entry.
 * @param directory The folder the entry's name is relative to.
 */
const char *sort_order_detail (path_t *directory, const entry_t *entry);

#endif /* SORT_ORDER_H__ */
