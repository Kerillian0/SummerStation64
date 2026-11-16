/**
 * @file ra_hook.c
 * @brief RetroAchievements experiment, step 1 (see ra_hook.h).
 */

#include "ra_hook.h"
#include "vr4300_asm.h"

#define MI_INTR_ADDRESS     (0xA4300008UL)  /* MI_INTR: which interrupts are pending */
#define MI_INTR_VI          (0x08)
#define PI_STATUS_ADDRESS   (0xA4600010UL)
#define PI_STATUS_BUSY      (0x03)          /* DMA busy, IO busy */
#define CART_UNCACHED(a)    (0xA0000000UL | (a))
#define MAGIC               (0x52414831UL)  /* "RAH1" */

static uint32_t armed_rdram = 0;
static int armed_words = 0;

void ra_hook_arm (uint32_t rdram, int words) {
    armed_rdram = rdram;
    armed_words = (words > RA_HOOK_MAX_WORDS) ? RA_HOOK_MAX_WORDS : words;
}

bool ra_hook_armed (void) {
    return armed_words > 0;
}

/* Wait until the PI is idle (k0 only): lui, lw, andi, bne back to the lui, nop. */
static io32_t *wait_pi (io32_t *p) {
    *p++ = I_LUI(REG_K0, A_BASE(PI_STATUS_ADDRESS));
    *p++ = I_LW(REG_K0, A_OFFSET(PI_STATUS_ADDRESS), REG_K0);
    *p++ = I_ANDI(REG_K0, REG_K0, PI_STATUS_BUSY);
    *p++ = I_BNE(REG_K0, REG_ZERO, -4);
    *p++ = I_NOP();
    return p;
}

/* Write k1 to the cart at `cart` (k0 is used). */
static io32_t *store_cart (io32_t *p, uint32_t cart) {
    p = wait_pi(p);
    uint32_t address = CART_UNCACHED(cart);
    *p++ = I_LUI(REG_K0, A_BASE(address));
    *p++ = I_SW(REG_K1, A_OFFSET(address), REG_K0);
    return p;
}

io32_t *ra_hook_emit (io32_t *p, uint32_t scratch) {
    if (!ra_hook_armed()) {
        return p;
    }

    /* Only on a VI interrupt: once a frame. Only k0 and k1 may be used here. */
    *p++ = I_LUI(REG_K0, A_BASE(MI_INTR_ADDRESS));
    *p++ = I_LW(REG_K0, A_OFFSET(MI_INTR_ADDRESS), REG_K0);
    *p++ = I_ANDI(REG_K0, REG_K0, MI_INTR_VI);
    io32_t *skip = p;
    *p++ = I_NOP();     /* the branch past the copy, written once its length is known */
    *p++ = I_NOP();

    /* The frame counter, kept in a spare RDRAM word. */
    *p++ = I_LUI(REG_K0, A_BASE(scratch));
    *p++ = I_LW(REG_K1, A_OFFSET(scratch), REG_K0);
    *p++ = I_ADDIU(REG_K1, REG_K1, 1);
    *p++ = I_SW(REG_K1, A_OFFSET(scratch), REG_K0);
    p = store_cart(p, RA_HOOK_CART + 4);

    *p++ = I_LUI(REG_K1, MAGIC >> 16);
    *p++ = I_ORI(REG_K1, REG_K1, MAGIC & 0xFFFF);
    p = store_cart(p, RA_HOOK_CART);

    for (int i = 0; i < armed_words; i++) {
        uint32_t from = armed_rdram + (i * 4);
        *p++ = I_LUI(REG_K0, A_BASE(from));
        *p++ = I_LW(REG_K1, A_OFFSET(from), REG_K0);
        p = store_cart(p, RA_HOOK_CART + 8 + (i * 4));
    }

    /* Not a VI interrupt: carry on after the copy. */
    *skip = I_BEQ(REG_K0, REG_ZERO, (int) (p - (skip + 1)));   /* counted from the delay slot, as in cheats.c */

    return p;
}
