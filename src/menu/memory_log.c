/**
 * @file memory_log.c
 * @brief Where the memory goes: log lines for the memory audit (see memory_log.h).
 */

#include <libdragon.h>

#include "memory_log.h"

/* The linker's marks round the program (libdragon's n64.ld). */
extern char __text_start[], __text_end[], __data_start[], __data_end[], __bss_start[], __bss_end[], end[];

static int last_used = -1;

int memory_log_used (void) {
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    return heap.used;
}

static int kb (int bytes) {
    return (bytes >= 0) ? (bytes + 512) / 1024 : -((-bytes + 512) / 1024);
}

static bool told_program = false;

/* The program's own size and the heap's: said with the first stage, since
   lines written before the cart is set up don't reach the USB log. */
static void tell_program (void) {
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    int code = (int) (__text_end - __text_start);
    int data = (int) (__data_end - __data_start);
    int bss = (int) (__bss_end - __bss_start);
    int program = (int) ((uintptr_t) end - 0x80000000UL);
    debugf("memory: %d KB in all; the program takes %d KB (code %d, tables and text %d, fixed buffers %d), the heap %d KB\n",
           (int) (get_memory_size() / 1024), kb(program), kb(code), kb(data), kb(bss), kb(heap.total));
    told_program = true;
}

void memory_log_begin (void) {
    last_used = memory_log_used();
}

void memory_log_step (const char *what) {
    if (!told_program) {
        tell_program();
        debugf("memory: at start %d KB of the heap in use\n", kb(last_used));
    }
    int used = memory_log_used();
    if (last_used >= 0) {
        debugf("memory: %-34s %+5d KB (in use %d KB)\n", what, kb(used - last_used), kb(used));
    }
    last_used = used;
}

void memory_log_since (int before, const char *what) {
    int used = memory_log_used();
    debugf("memory: %-34s %+5d KB (in use %d KB)\n", what, kb(used - before), kb(used));
}
