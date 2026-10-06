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
    SORT_COUNT
} sort_order_t;

/** Label for the Settings screen. */
const char *sort_order_name (int order);

/**
 * Sort the browser's file list by the player's chosen order.
 * @param stock_compare The browser's own comparison, used for SORT_BY_TYPE.
 */
void sort_order_apply (menu_t *menu, int (*stock_compare) (const void *, const void *));

#endif /* SORT_ORDER_H__ */
