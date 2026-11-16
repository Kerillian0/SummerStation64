/**
 * @file sc64_link.h
 * @brief Talking to the SummerCart64 over its USB cable from Windows.
 *
 * The cart shows up as a serial port. Its protocol (SummerCart64
 * docs/03_usb_interface.md): a command is "CMD", one ID character and two
 * big-endian uint32 arguments, then any data; the answer is "CMP" (or "ERR"),
 * the same ID, a big-endian uint32 length, then the data. Addresses are the
 * cart's own: its 64 MB memory starts at 0 (the N64 sees it at 0x10000000).
 */

#ifndef SC64_LINK_H__
#define SC64_LINK_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Open the cart's port (`port` like "COM3", or NULL to find it) and reset the link. */
bool sc64_open (const char *port);

/** Close the port. */
void sc64_close (void);

/** The port in use, e.g. "COM3". */
const char *sc64_port (void);

/** 'v': the cart's identifier, "SCv2" (4 bytes + NUL). */
bool sc64_identify (char out[5]);

/** 'm': read `length` bytes of the cart's memory at `address`. */
bool sc64_read (uint32_t address, void *out, uint32_t length);

/** 'M': write `length` bytes to the cart's memory at `address`. */
bool sc64_write (uint32_t address, const void *data, uint32_t length);

/** What went wrong last, for the user. */
const char *sc64_error (void);

#endif
