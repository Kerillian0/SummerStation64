/**
 * @file sc64.h
 * @brief SC64 flashcart support
 * @ingroup flashcart 
 */

#ifndef FLASHCART_SC64_H__
#define FLASHCART_SC64_H__


#include "../flashcart.h"


/**
 * @addtogroup sc64
 * @{
 */

flashcart_t *sc64_get_flashcart (void);

/** SummerStation64: keep the cart's memory writable while the game runs (the RetroAchievements experiment, boot/ra_hook.h). */
extern bool sc64_keep_rom_write;

/** @} */ /* sc64 */


#endif
