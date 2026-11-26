/**
 * @file memory_log.h
 * @brief Where the memory goes: log lines for the memory audit.
 *
 * At start-up the log says what the program itself takes (code, fixed
 * tables and buffers) and then, stage by stage, how much of the heap each
 * part of the start-up took ("memory: screen buffers +600 KB"). Big loads
 * later on (the Games list, the theme's background, the title fonts, the
 * assistant's pictures) say what they cost where they happen. Log only:
 * nothing on screen changes.
 */

#ifndef MEMORY_LOG_H__
#define MEMORY_LOG_H__

/** At the very start: the program's own size, and the heap's starting point. */
void memory_log_begin (void);

/** A start-up stage has finished: how much it took since the last stage. */
void memory_log_step (const char *what);

/** The heap in use now, in bytes, to pass to memory_log_since() later. */
int memory_log_used (void);

/** What a load took: the heap in use now against `before`. */
void memory_log_since (int before, const char *what);

#endif /* MEMORY_LOG_H__ */
