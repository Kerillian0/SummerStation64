/**
 * @file ra_hook.c
 * @brief RetroAchievements experiment, step 2d (see ra_hook.h).
 *
 * The loop is written as machine code at boot, like the rest of the cheat
 * engine. Inside the exception handler only k0 and k1 are free, so t0-t6 are
 * saved to scratch RDRAM first and put back before the game's own handler
 * runs. Registers while copying: t0 the list (cart), t1 bytes left in the
 * slot, t2 entries left, t3 where the slot is written (cart), t4 the entry's
 * first word, then where a copy reads (RDRAM, cached), t5 its second word
 * (bytes left to copy), t6 the base B; k1 the address or word in flight, k0
 * flags and the PI waits.
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
#define MAGIC_HOOK          (0x52414833UL)  /* "RAH3" */
#define MAGIC_LIST          (0x52414C32UL)  /* "RAL2" */

/* Shift left: the encoding NOP uses (sll zero, zero, 0). */
#define I_SLL(rd, rt, sa)   __ASM_R_INST(OP_SPECIAL, 0, rt, rd, sa, FUNCT_SSL)
#define I_ADDU(rd, rs, rt)  __ASM_R_INST(OP_SPECIAL, rs, rt, rd, 0, FUNCT_ADDU)
#define I_BLTZ(rs, offset)  __ASM_I_INST(OP_REGIMM, rs, REGIMM_BLTZ, offset)
#define I_BLEZ(rs, offset)  __ASM_I_INST(OP_BLEZ, rs, 0, offset)
#define I_XORI(rt, rs, imm) __ASM_I_INST(OP_XORI, rs, rt, imm)

/* Scratch RDRAM layout. */
#define SAVED(n)            ((n) * 4)       /* t0-t6 */
#define FRAME               (28)
#define SLOT_START          (32)

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
typedef enum { L_END, L_RESTORE, L_ENTRIES, L_POINTER, L_POINTER_GOT, L_OUTSIDE, L_DONE, L_WORDS, L_CHECK_ROOM, L_BODY, L_COUNT } label_t;
typedef enum { B_BEQ, B_BNE, B_BGTZ, B_BLTZ, B_BLEZ } branch_t;

static io32_t *label_at[L_COUNT];
static struct { io32_t *at; branch_t kind; int rs, rt; label_t to; } fixes[32];
static int fix_count;

static uint32_t branch_word (branch_t kind, int rs, int rt, int offset) {
    switch (kind) {
        case B_BNE: return I_BNE(rs, rt, offset);
        case B_BGTZ: return I_BGTZ(rs, offset);
        case B_BLTZ: return I_BLTZ(rs, offset);
        case B_BLEZ: return I_BLEZ(rs, offset);
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

/* reg = reg & mask, with k0 holding the mask. */
static io32_t *mask (io32_t *p, int reg, uint32_t value) {
    *p++ = I_LUI(REG_K0, value >> 16);
    *p++ = I_ORI(REG_K0, REG_K0, value & 0xFFFF);
    *p++ = I_AND(reg, reg, REG_K0);
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
    const int t[7] = { REG_T0, REG_T0 + 1, REG_T0 + 2, REG_T0 + 3, REG_T0 + 4, REG_T0 + 5, REG_T0 + 6 };

    /* Once a frame: only on a VI interrupt. */
    *p++ = I_LUI(REG_K0, A_BASE(MI_INTR_ADDRESS));
    *p++ = I_LW(REG_K0, A_OFFSET(MI_INTR_ADDRESS), REG_K0);
    *p++ = I_ANDI(REG_K0, REG_K0, MI_INTR_VI);
    p = branch(p, B_BEQ, REG_K0, REG_ZERO, L_END);

    /* Save t0-t6; count the frame. */
    *p++ = I_LUI(REG_K0, A_BASE(scratch));
    for (int i = 0; i < 7; i++) {
        *p++ = I_SW(t[i], A_OFFSET(scratch + SAVED(i)), REG_K0);
    }
    *p++ = I_LW(REG_K1, A_OFFSET(scratch + FRAME), REG_K0);
    *p++ = I_ADDIU(REG_K1, REG_K1, 1);
    *p++ = I_SW(REG_K1, A_OFFSET(scratch + FRAME), REG_K0);

    /* Status: the frame counter, then "RAH3". */
    *p++ = I_LUI(t[0], A_BASE(CART(RA_HOOK_STATUS)));
    p = wait_pi(p);
    *p++ = I_SW(REG_K1, A_OFFSET(CART(RA_HOOK_STATUS)) + 4, t[0]);
    *p++ = I_LUI(REG_K1, MAGIC_HOOK >> 16);
    *p++ = I_ORI(REG_K1, REG_K1, MAGIC_HOOK & 0xFFFF);
    p = wait_pi(p);
    *p++ = I_SW(REG_K1, A_OFFSET(CART(RA_HOOK_STATUS)), t[0]);

    /* The PC's list: nothing to copy until it says "RAL2". */
    *p++ = I_LUI(t[0], A_BASE(CART(RA_HOOK_LIST)));
    *p++ = I_ADDIU(t[0], t[0], A_OFFSET(CART(RA_HOOK_LIST)));
    p = wait_pi(p);
    *p++ = I_LW(t[1], 0, t[0]);
    *p++ = I_LUI(REG_K1, MAGIC_LIST >> 16);
    *p++ = I_ORI(REG_K1, REG_K1, MAGIC_LIST & 0xFFFF);
    p = branch(p, B_BNE, t[1], REG_K1, L_RESTORE);
    p = wait_pi(p);
    *p++ = I_LW(t[2], 4, t[0]);
    *p++ = I_ANDI(t[2], t[2], RA_HOOK_MAX_ENTRIES);
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
    *p++ = I_OR(t[6], REG_ZERO, REG_ZERO);              /* B = 0 */

    /* Each entry: k1 = B + its address. */
    label(p, L_ENTRIES);
    p = branch(p, B_BEQ, t[2], REG_ZERO, L_DONE);
    p = wait_pi(p);
    *p++ = I_LW(t[4], 0, t[0]);
    p = wait_pi(p);
    *p++ = I_LW(t[5], 4, t[0]);
    *p++ = I_ADDIU(t[0], t[0], 8);
    *p++ = I_ADDIU(t[2], t[2], -1);
    *p++ = I_OR(REG_K1, t[4], REG_ZERO);
    p = mask(p, REG_K1, 0x007FFFFF);
    *p++ = I_ADDU(REG_K1, REG_K1, t[6]);
    p = branch(p, B_BLTZ, t[4], 0, L_POINTER);

    /* A copy: from k1's word (cached, as the game sees it), its address first if asked. */
    p = mask(p, REG_K1, 0x007FFFFC);
    *p++ = I_OR(t[6], REG_ZERO, REG_ZERO);              /* B = 0 after a copy */
    *p++ = I_LUI(REG_K0, RA_HOOK_TELL >> 16);
    *p++ = I_AND(REG_K0, t[4], REG_K0);                 /* k0 = the TELL flag */
    *p++ = I_LUI(t[4], 0x8000);
    *p++ = I_OR(t[4], REG_K1, t[4]);                    /* t4 = where to read */
    p = branch(p, B_BEQ, REG_K0, REG_ZERO, L_WORDS);
    p = branch(p, B_BLEZ, t[1], 0, L_DONE);             /* no room for the address */
    p = wait_pi(p);
    *p++ = I_SW(REG_K1, 0, t[3]);                       /* k1: the RDRAM address it reads */
    *p++ = I_ADDIU(t[3], t[3], 4);
    *p++ = I_ADDIU(t[1], t[1], -4);
    p = branch(p, B_BEQ, REG_ZERO, REG_ZERO, L_WORDS);

    /* A pointer: B = the 24-bit value at k1 as RetroAchievements reads it (bytes k1, k1 + 1,
       k1 + 2 of its swapped view, low first: the console's bytes at each ^ 3), 0 past the end
       of RDRAM; or last frame's, kept in the entry. */
    label(p, L_POINTER);
    *p++ = I_SRL(REG_K0, REG_K1, 23);
    p = branch(p, B_BNE, REG_K0, REG_ZERO, L_OUTSIDE);
    *p++ = I_LUI(REG_K0, 0x8000);
    *p++ = I_OR(REG_K1, REG_K1, REG_K0);                /* cached, as the game sees it */
    *p++ = I_OR(t[6], REG_ZERO, REG_ZERO);
    for (int i = 2; i >= 0; i--) {
        *p++ = I_ADDIU(REG_K0, REG_K1, i);
        *p++ = I_XORI(REG_K0, REG_K0, 3);
        *p++ = I_LBU(REG_K0, 0, REG_K0);
        *p++ = I_SLL(t[6], t[6], 8);
        *p++ = I_OR(t[6], t[6], REG_K0);
    }
    label(p, L_POINTER_GOT);
    *p++ = I_OR(REG_K1, t[6], REG_ZERO);                /* k1 = this frame's pointer */
    *p++ = I_LUI(REG_K0, RA_HOOK_LAST >> 16);
    *p++ = I_AND(REG_K0, t[4], REG_K0);
    p = branch(p, B_BEQ, REG_K0, REG_ZERO, L_ENTRIES);
    *p++ = I_OR(t[6], t[5], REG_ZERO);                  /* last frame's ... */
    p = wait_pi(p);
    *p++ = I_SW(REG_K1, -4, t[0]);                      /* ... and this frame's kept for the next */
    p = branch(p, B_BEQ, REG_ZERO, REG_ZERO, L_ENTRIES);
    label(p, L_OUTSIDE);
    *p++ = I_OR(t[6], REG_ZERO, REG_ZERO);
    p = branch(p, B_BEQ, REG_ZERO, REG_ZERO, L_POINTER_GOT);

    /* Each word of a copy, while it and the slot have room. */
    label(p, L_WORDS);
    p = branch(p, B_BGTZ, t[5], 0, L_CHECK_ROOM);
    p = branch(p, B_BEQ, REG_ZERO, REG_ZERO, L_ENTRIES);
    label(p, L_CHECK_ROOM);
    p = branch(p, B_BGTZ, t[1], 0, L_BODY);
    p = branch(p, B_BEQ, REG_ZERO, REG_ZERO, L_DONE);
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
    label(p, L_DONE);
    *p++ = I_LUI(REG_K0, A_BASE(scratch));
    *p++ = I_LW(t[3], A_OFFSET(scratch + SLOT_START), REG_K0);
    *p++ = I_LW(REG_K1, A_OFFSET(scratch + FRAME), REG_K0);
    p = wait_pi(p);
    *p++ = I_SW(REG_K1, 0, t[3]);

    /* Put t0-t6 back. */
    label(p, L_RESTORE);
    *p++ = I_LUI(REG_K0, A_BASE(scratch));
    for (int i = 0; i < 7; i++) {
        *p++ = I_LW(t[i], A_OFFSET(scratch + SAVED(i)), REG_K0);
    }

    label(p, L_END);
    for (int i = 0; i < fix_count; i++) {
        io32_t *at = fixes[i].at;
        *at = branch_word(fixes[i].kind, fixes[i].rs, fixes[i].rt, (int) (label_at[fixes[i].to] - (at + 1)));
    }
    return p;
}
