/**
 * @file ra_hook.c
 * @brief RetroAchievements experiment, step 2a (see ra_hook.h).
 *
 * The loop is written as machine code at boot, like the rest of the cheat
 * engine. Inside the exception handler only k0 and k1 are free, so t0-t5 are
 * saved to scratch RDRAM first and put back before the game's own handler
 * runs. Registers while copying: t0 the list (cart), t1 bytes left in the
 * slot, t2 ranges left, t3 where the slot is written (cart), t4 where the
 * range is read (RDRAM, cached), t5 bytes left in the range; k1 the word in
 * flight, k0 the PI waits.
 */

#include <string.h>
#ifndef RA_HOOK_PC_TEST
#include <libdragon.h>
#endif
#include "ra_hook.h"
#include "vr4300_asm.h"

#define MI_INTR_ADDRESS     (0xA4300008UL)  /* MI_INTR: which interrupts are pending */
#define MI_INTR_VI          (0x08)
#define PI_STATUS_ADDRESS   (0xA4600010UL)
#define PI_STATUS_BUSY      (0x03)          /* DMA busy, IO busy */
#define CART(a)             (0xA0000000UL | (a))
#define MAGIC_HOOK          (0x52414832UL)  /* "RAH2" */
#define MAGIC_LIST          (0x52414C31UL)  /* "RAL1" */

/* Shift left: the encoding NOP uses (sll zero, zero, 0). */
#define I_SLL(rd, rt, sa)   __ASM_R_INST(OP_SPECIAL, 0, rt, rd, sa, FUNCT_SSL)

/* Scratch RDRAM layout. */
#define SAVED(n)            ((n) * 4)       /* t0-t5 */
#define FRAME               (24)
#define SLOT_START          (28)

static bool armed = false;

void ra_hook_arm (bool on) {
    armed = on;
}

bool ra_hook_armed (void) {
    return armed;
}

#ifndef RA_HOOK_PC_TEST
void ra_hook_note_game (const char *path, uint32_t rom_size, const char game_code[4]) {
    io_write(RA_HOOK_LIST, 0);          /* an old game's copy list must not run in this one */
    io_write(RA_HOOK_STATUS, 0);
    io_write(RA_HOOK_GAME, 0);          /* not valid while it changes */
    io_write(RA_HOOK_GAME + 4, rom_size);
    io_write(RA_HOOK_GAME + 8, ((uint32_t) (uint8_t) game_code[0] << 24) | ((uint32_t) (uint8_t) game_code[1] << 16)
                               | ((uint32_t) (uint8_t) game_code[2] << 8) | (uint8_t) game_code[3]);
    uint8_t name[244] = { 0 };
    strncpy((char *) name, path, sizeof(name) - 1);
    for (int i = 0; i < (int) sizeof(name); i += 4) {
        io_write(RA_HOOK_GAME + 12 + i, ((uint32_t) name[i] << 24) | ((uint32_t) name[i + 1] << 16) | ((uint32_t) name[i + 2] << 8) | name[i + 3]);
    }
    io_write(RA_HOOK_GAME, 0x52414731UL);   /* "RAG1", last */
}
#endif

/* Branches to places further on are written once those are known. */
typedef enum { L_END, L_RESTORE, L_RANGES, L_RANGES_DONE, L_WORDS, L_CHECK_ROOM, L_BODY, L_COUNT } label_t;
typedef enum { B_BEQ, B_BNE, B_BGTZ } branch_t;

static io32_t *label_at[L_COUNT];
static struct { io32_t *at; branch_t kind; int rs, rt; label_t to; } fixes[16];
static int fix_count;

static uint32_t branch_word (branch_t kind, int rs, int rt, int offset) {
    switch (kind) {
        case B_BNE: return I_BNE(rs, rt, offset);
        case B_BGTZ: return I_BGTZ(rs, offset);
        default: return I_BEQ(rs, rt, offset);
    }
}

/* A branch and its delay slot (a nop); offsets count from the delay slot. */
static io32_t *branch (io32_t *p, branch_t kind, int rs, int rt, label_t to) {
    if (label_at[to]) {
        *p = branch_word(kind, rs, rt, (int) (label_at[to] - (p + 1)));
    } else if (fix_count < (int) (sizeof(fixes) / sizeof(fixes[0]))) {
        fixes[fix_count].at = p;
        fixes[fix_count].kind = kind;
        fixes[fix_count].rs = rs;
        fixes[fix_count].rt = rt;
        fixes[fix_count].to = to;
        fix_count++;
        *p = I_NOP();
    }
    p++;
    *p++ = I_NOP();
    return p;
}

static void label (io32_t *p, label_t name) {
    label_at[name] = p;
}

/* Wait until the PI is idle (k0 only). */
static io32_t *wait_pi (io32_t *p) {
    *p++ = I_LUI(REG_K0, A_BASE(PI_STATUS_ADDRESS));
    *p++ = I_LW(REG_K0, A_OFFSET(PI_STATUS_ADDRESS), REG_K0);
    *p++ = I_ANDI(REG_K0, REG_K0, PI_STATUS_BUSY);
    *p++ = I_BNE(REG_K0, REG_ZERO, -4);
    *p++ = I_NOP();
    return p;
}

io32_t *ra_hook_emit (io32_t *p, uint32_t scratch) {
    if (!armed) {
        return p;
    }
    for (int i = 0; i < L_COUNT; i++) {
        label_at[i] = NULL;
    }
    fix_count = 0;
    const int t[6] = { REG_T0, REG_T0 + 1, REG_T0 + 2, REG_T0 + 3, REG_T0 + 4, REG_T0 + 5 };

    /* Once a frame: only on a VI interrupt. */
    *p++ = I_LUI(REG_K0, A_BASE(MI_INTR_ADDRESS));
    *p++ = I_LW(REG_K0, A_OFFSET(MI_INTR_ADDRESS), REG_K0);
    *p++ = I_ANDI(REG_K0, REG_K0, MI_INTR_VI);
    p = branch(p, B_BEQ, REG_K0, REG_ZERO, L_END);

    /* Save t0-t5; count the frame. */
    *p++ = I_LUI(REG_K0, A_BASE(scratch));
    for (int i = 0; i < 6; i++) {
        *p++ = I_SW(t[i], A_OFFSET(scratch + SAVED(i)), REG_K0);
    }
    *p++ = I_LW(REG_K1, A_OFFSET(scratch + FRAME), REG_K0);
    *p++ = I_ADDIU(REG_K1, REG_K1, 1);
    *p++ = I_SW(REG_K1, A_OFFSET(scratch + FRAME), REG_K0);

    /* Status: the frame counter, then "RAH2". */
    *p++ = I_LUI(t[0], A_BASE(CART(RA_HOOK_STATUS)));
    p = wait_pi(p);
    *p++ = I_SW(REG_K1, A_OFFSET(CART(RA_HOOK_STATUS)) + 4, t[0]);
    *p++ = I_LUI(REG_K1, MAGIC_HOOK >> 16);
    *p++ = I_ORI(REG_K1, REG_K1, MAGIC_HOOK & 0xFFFF);
    p = wait_pi(p);
    *p++ = I_SW(REG_K1, A_OFFSET(CART(RA_HOOK_STATUS)), t[0]);

    /* The PC's list: nothing to copy until it says "RAL1". */
    *p++ = I_LUI(t[0], A_BASE(CART(RA_HOOK_LIST)));
    *p++ = I_ADDIU(t[0], t[0], A_OFFSET(CART(RA_HOOK_LIST)));
    p = wait_pi(p);
    *p++ = I_LW(t[1], 0, t[0]);
    *p++ = I_LUI(REG_K1, MAGIC_LIST >> 16);
    *p++ = I_ORI(REG_K1, REG_K1, MAGIC_LIST & 0xFFFF);
    p = branch(p, B_BNE, t[1], REG_K1, L_RESTORE);
    p = wait_pi(p);
    *p++ = I_LW(t[2], 4, t[0]);
    *p++ = I_ANDI(t[2], t[2], RA_HOOK_MAX_RANGES);
    *p++ = I_ADDIU(t[0], t[0], 8);

    /* This frame's slot: RING + (frame % SLOTS) * SLOT_SIZE (RING's low half is 0). */
    *p++ = I_LUI(REG_K0, A_BASE(scratch));
    *p++ = I_LW(t[3], A_OFFSET(scratch + FRAME), REG_K0);
    *p++ = I_ANDI(t[3], t[3], RA_HOOK_SLOTS - 1);
    *p++ = I_SLL(t[3], t[3], 12);
    *p++ = I_LUI(REG_K1, CART(RA_HOOK_RING) >> 16);
    *p++ = I_OR(t[3], t[3], REG_K1);
    *p++ = I_SW(t[3], A_OFFSET(scratch + SLOT_START), REG_K0);
    *p++ = I_ADDIU(t[3], t[3], 4);
    *p++ = I_ORI(t[1], REG_ZERO, RA_HOOK_SLOT_SIZE - 4);

    /* Each range: its address (made a cached RDRAM address) and length. */
    label(p, L_RANGES);
    p = branch(p, B_BEQ, t[2], REG_ZERO, L_RANGES_DONE);
    p = wait_pi(p);
    *p++ = I_LW(t[4], 0, t[0]);
    p = wait_pi(p);
    *p++ = I_LW(t[5], 4, t[0]);
    *p++ = I_ADDIU(t[0], t[0], 8);
    *p++ = I_ADDIU(t[2], t[2], -1);
    *p++ = I_LUI(REG_K1, 0x807F);
    *p++ = I_ORI(REG_K1, REG_K1, 0xFFFC);
    *p++ = I_AND(t[4], t[4], REG_K1);       /* inside RDRAM, word aligned ... */
    *p++ = I_LUI(REG_K1, 0x8000);
    *p++ = I_OR(t[4], t[4], REG_K1);        /* ... and cached, as the game sees it */

    /* Each word of it, while the range and the slot have room. */
    label(p, L_WORDS);
    p = branch(p, B_BGTZ, t[5], 0, L_CHECK_ROOM);
    p = branch(p, B_BEQ, REG_ZERO, REG_ZERO, L_RANGES);
    label(p, L_CHECK_ROOM);
    p = branch(p, B_BGTZ, t[1], 0, L_BODY);
    p = branch(p, B_BEQ, REG_ZERO, REG_ZERO, L_RANGES_DONE);
    label(p, L_BODY);
    *p++ = I_LW(REG_K1, 0, t[4]);
    p = wait_pi(p);
    *p++ = I_SW(REG_K1, 0, t[3]);
    *p++ = I_ADDIU(t[4], t[4], 4);
    *p++ = I_ADDIU(t[3], t[3], 4);
    *p++ = I_ADDIU(t[5], t[5], -4);
    *p++ = I_ADDIU(t[1], t[1], -4);
    p = branch(p, B_BEQ, REG_ZERO, REG_ZERO, L_WORDS);

    /* The slot is complete: its frame number goes in last. */
    label(p, L_RANGES_DONE);
    *p++ = I_LUI(REG_K0, A_BASE(scratch));
    *p++ = I_LW(t[3], A_OFFSET(scratch + SLOT_START), REG_K0);
    *p++ = I_LW(REG_K1, A_OFFSET(scratch + FRAME), REG_K0);
    p = wait_pi(p);
    *p++ = I_SW(REG_K1, 0, t[3]);

    /* Put t0-t5 back. */
    label(p, L_RESTORE);
    *p++ = I_LUI(REG_K0, A_BASE(scratch));
    for (int i = 0; i < 6; i++) {
        *p++ = I_LW(t[i], A_OFFSET(scratch + SAVED(i)), REG_K0);
    }

    label(p, L_END);
    for (int i = 0; i < fix_count; i++) {
        io32_t *at = fixes[i].at;
        *at = branch_word(fixes[i].kind, fixes[i].rs, fixes[i].rt, (int) (label_at[fixes[i].to] - (at + 1)));
    }
    return p;
}
