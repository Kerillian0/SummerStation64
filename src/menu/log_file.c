/**
 * @file log_file.c
 * @brief The menu's log, kept in memory, saved to the SD card for a bug report.
 */

#include <libdragon.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "log_file.h"
#include "safe_file.h"

#ifndef MENU_VERSION
#define MENU_VERSION "Unknown"
#endif

#define RING_SIZE       (8 * 1024)    /* about the last 100 lines */

/* Spare cart memory for a crash's lines: just below the art file's index
   (baked_art.c, 0x13000000) and far from the USB log's area (0x13800000 up).
   The menu file itself sits at the bottom of cart memory. */
#define CRASH_PI        (0x12FF0000UL)
#define CRASH_MAGIC     (0x4C4F4731UL)  /* "LOG1" */

static char ring[RING_SIZE];
static size_t head = 0;         /* where the next byte goes */
static bool wrapped = false;    /* the ring has gone round: the oldest byte is at head */
static bool crash_saved = false;

int __real_write (int file, char *ptr, int len);

/* Every write to stderr (debugf) is also kept in the ring. */
int __wrap_write (int file, char *ptr, int len) {
    if (file == 2 && ptr && len > 0) {
        for (int i = 0; i < len; i++) {
            ring[head++] = ptr[i];
            if (head == RING_SIZE) {
                head = 0;
                wrapped = true;
            }
        }
    }
    return __real_write(file, ptr, len);
}

/* The kept text in order, oldest first, from the start of a whole line.
   Calls `out` once or twice (the ring's two halves). */
static void ring_each (void (*out) (const char *text, size_t length, void *arg), void *arg) {
    if (!wrapped) {
        out(ring, head, arg);
        return;
    }
    size_t from = head;
    size_t skipped = 0;
    while (skipped < RING_SIZE && ring[from] != '\n') {     /* the oldest line is cut short: leave it out */
        from = (from + 1) % RING_SIZE;
        skipped++;
    }
    from = (from + 1) % RING_SIZE;
    if (from >= head) {
        out(ring + from, RING_SIZE - from, arg);
        out(ring, head, arg);
    } else {
        out(ring + from, head - from, arg);
    }
}

static void to_file (const char *text, size_t length, void *arg) {
    if (length > 0) {
        fwrite(text, 1, length, (FILE *) arg);
    }
}

static bool write_file (const char *storage_prefix, const char *name, const char *heading,
                        void (*each) (void (*out) (const char *, size_t, void *), void *)) {
    char path[96], temp[96];
    snprintf(path, sizeof(path), "%smenu/%s.txt", storage_prefix, name);
    snprintf(temp, sizeof(temp), "%smenu/%s.tmp", storage_prefix, name);
    FILE *f = fopen(temp, "w");
    if (!f) {
        return false;
    }
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(f, "%s\n", heading);
    fprintf(f, "Menu: %s\n", MENU_VERSION);
    if (t) {
        fprintf(f, "Saved: %04d-%02d-%02d %02d:%02d\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);
    }
    fprintf(f, "Memory: %s\n\n", is_memory_expanded() ? "Expansion Pak (8MB)" : "4MB");
    each(to_file, f);
    bool ok = (fclose(f) == 0) && safe_file_replace(temp, path);
    return ok;
}

bool log_file_save (const char *storage_prefix) {
    debugf("log file: saving %s\n", wrapped ? "the last 8 KB" : "everything since start-up");
    return write_file(storage_prefix, "log", "SummerStation64 log (Settings > Console > About > Save Log)", ring_each);
}

/* ---------- a crash: through cart memory to the next start ---------- */

static uint32_t crash_at = 0;   /* the next cart address to write */

static void to_cart (const char *text, size_t length, void *arg) {
    (void) arg;
    /* Four bytes at a time, big-endian like the processor; a last part word padded with spaces. */
    for (size_t i = 0; i < length && crash_at < CRASH_PI + 8 + RING_SIZE; i += 4) {
        uint32_t word = 0;
        for (size_t k = 0; k < 4; k++) {
            word = (word << 8) | (uint8_t) ((i + k < length) ? text[i + k] : ' ');
        }
        io_write(crash_at, word);
        crash_at += 4;
    }
}

void log_file_crash (void) {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    crash_at = CRASH_PI + 8;
    ring_each(to_cart, NULL);
    io_write(CRASH_PI + 4, crash_at - (CRASH_PI + 8));
    io_write(CRASH_PI, CRASH_MAGIC);      /* last: a half-written copy is never taken for a whole one */
}

/* The crash's lines, read back from cart memory a word at a time. */
static uint32_t rescue_length = 0;

static void from_cart (void (*out) (const char *, size_t, void *), void *arg) {
    char piece[256];
    for (uint32_t at = 0; at < rescue_length; at += sizeof(piece)) {
        uint32_t n = (rescue_length - at < sizeof(piece)) ? rescue_length - at : sizeof(piece);
        for (uint32_t i = 0; i < n; i += 4) {
            uint32_t word = io_read(CRASH_PI + 8 + at + i);
            piece[i] = (char) (word >> 24);
            piece[i + 1] = (char) (word >> 16);
            piece[i + 2] = (char) (word >> 8);
            piece[i + 3] = (char) word;
        }
        out(piece, n, arg);
    }
}

void log_file_rescue (const char *storage_prefix) {
    if (io_read(CRASH_PI) != CRASH_MAGIC) {
        return;
    }
    rescue_length = io_read(CRASH_PI + 4);
    if (rescue_length == 0 || rescue_length > RING_SIZE || (rescue_length & 3)) {
        io_write(CRASH_PI, 0);
        return;
    }
    crash_saved = write_file(storage_prefix, "crash-log", "SummerStation64 log from before a crash (saved at the next start)", from_cart);
    io_write(CRASH_PI, 0);      /* saved once, never again */
    debugf("log file: a crash's log (%d bytes) %s menu/crash-log.txt\n", (int) rescue_length, crash_saved ? "saved to" : "could not be saved to");
}

/* A failed check (assert) goes straight to libdragon's own crash screen,
   after printing what failed to the log: keep the lines first. */
void __real___inspector_assertion (const char *failedexpr, const char *msg, va_list args);

void __wrap___inspector_assertion (const char *failedexpr, const char *msg, va_list args) {
    log_file_crash();
    __real___inspector_assertion(failedexpr, msg, args);
}

bool log_file_crash_saved (void) {
    return crash_saved;
}
