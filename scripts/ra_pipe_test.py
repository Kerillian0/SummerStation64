#!/usr/bin/env python3
"""RetroAchievements experiment, step 1: read the game's memory over USB.

With the hidden option `ra_test = 1` in sd:/menu/options.ini and Super Mario
64 (USA) started on a console with the Expansion Pak, the menu's hook copies
1 KB of the game's memory to the SummerCart64 every frame (src/boot/ra_hook.h).
This script reads that copy a few times a second over the SC64's USB cable
and prints Mario's coins, stars, lives and health, plus the hook's frame
counter (it should climb by about 60 a second on NTSC, 50 on PAL).

    pip install pyserial
    python ra_pipe_test.py            (finds the SC64 by itself)
    python ra_pipe_test.py --port COM5

Close sc64deployer (and anything else using the cart's USB port) first.
Ctrl+C stops it. Nothing is written to the cart: it only reads.

Protocol (SummerCart64 docs/03_usb_interface.md): a command is "CMD", one ID
character, two big-endian uint32 arguments, then any data; the answer is
"CMP" (or "ERR"), the same ID, a big-endian uint32 length, then the data.
'v' answers "SCv2"; 'm' (MEMORY_READ) takes an address and a length. The
cart's memory starts at 0 from USB; the hook writes at 0x03B00000 (the N64
sees it at 0x13B00000).
"""
import argparse
import struct
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit("pyserial is missing: run  pip install pyserial")

COPY_ADDRESS = 0x03B00000       # RA_HOOK_CART as seen from USB
COPY_WORDS = 256                # the 1 KB the hook copies, from 0x8033B000
MAGIC = b"RAH1"
GAME_BASE = 0x8033B000
# Super Mario 64 (USA): Mario's state (the addresses the well-known GameShark codes use)
COINS, STARS, LIVES, HEALTH = 0x8033B218, 0x8033B21A, 0x8033B21D, 0x8033B21E


def find_port():
    for port in serial.tools.list_ports.comports():
        text = f"{port.description} {port.product or ''} {port.manufacturer or ''}"
        if "SC64" in text or "SummerCart" in text or (port.vid == 0x0403 and port.pid == 0x6014):
            return port.device
    names = ", ".join(f"{p.device} ({p.description})" for p in serial.tools.list_ports.comports()) or "none"
    sys.exit(f"No SummerCart64 found among the serial ports ({names}). Is the USB cable in? Try --port COMx.")


def wait_dsr(link, state, seconds=2.0):
    end = time.time() + seconds
    while link.dsr != state:
        if time.time() > end:
            sys.exit(f"The cart did not answer the reset handshake (DSR never went {'high' if state else 'low'}).")
        time.sleep(0.001)


def reset(link):
    """The handshake from the SC64 docs: DTR high, wait for DSR high, clear, DTR low, wait for DSR low."""
    link.dtr = True
    wait_dsr(link, True)
    link.reset_input_buffer()
    link.reset_output_buffer()
    link.dtr = False
    wait_dsr(link, False)


def read_exact(link, count):
    data = b""
    end = time.time() + 2.0
    while len(data) < count:
        piece = link.read(count - len(data))
        if piece:
            data += piece
        elif time.time() > end:
            raise TimeoutError(f"waited for {count} bytes, got {len(data)}")
    return data


def command(link, cmd_id, arg0=0, arg1=0, data=b""):
    link.write(b"CMD" + cmd_id + struct.pack(">II", arg0, arg1) + data)
    while True:
        head = read_exact(link, 4)
        length = struct.unpack(">I", read_exact(link, 4))[0]
        payload = read_exact(link, length) if length else b""
        if head[:3] == b"PKT":
            continue    # something the N64 side sent on its own (debug output): not our answer
        if head[:3] == b"ERR":
            raise RuntimeError(f"the cart answered ERR to '{cmd_id.decode()}'")
        if head[:3] == b"CMP" and head[3:4] == cmd_id:
            return payload
        raise RuntimeError(f"unexpected answer {head!r}")


def word_at(copy, address, size, signed=True):
    offset = 8 + (address - GAME_BASE)
    return int.from_bytes(copy[offset:offset + size], "big", signed=signed)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--port", help="the SC64's serial port (COM5, /dev/ttyUSB0, ...)")
    parser.add_argument("--every", type=float, default=0.25, help="seconds between reads (default 0.25)")
    args = parser.parse_args()

    port = args.port or find_port()
    print(f"Opening {port}...")
    link = serial.Serial(port, timeout=0.5, write_timeout=2.0)
    reset(link)
    ident = command(link, b"v")
    print(f"Cart says {ident!r} (expected b'SCv2')")

    last_counter = None
    last_time = None
    last_copy = None
    print("Reading the hook's copy. Start Super Mario 64 (USA) if it isn't running yet. Ctrl+C stops.\n")
    try:
        while True:
            copy = command(link, b"m", COPY_ADDRESS, 8 + COPY_WORDS * 4)
            now = time.time()
            if copy[:4] != MAGIC:
                print(f"\rNo copy yet (the cart holds {copy[:4]!r} there; the hook hasn't run).        ", end="", flush=True)
            else:
                counter = int.from_bytes(copy[4:8], "big")
                rate = ""
                if last_counter is not None and now > last_time:
                    rate = f"{(counter - last_counter) / (now - last_time):5.1f} frames/s"
                changed = 0
                if last_copy is not None:
                    changed = sum(1 for i in range(8, len(copy), 4) if copy[i:i + 4] != last_copy[i:i + 4])
                health = word_at(copy, HEALTH, 2) >> 8
                print(f"\rframe {counter:8d}  {rate:>15}  coins {word_at(copy, COINS, 2):4d}  "
                      f"stars {word_at(copy, STARS, 2):3d}  lives {word_at(copy, LIVES, 1):3d}  "
                      f"health {health:2d}/8  ({changed:3d} words changed)   ", end="", flush=True)
                last_counter, last_time, last_copy = counter, now, copy
            time.sleep(args.every)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        link.close()


if __name__ == "__main__":
    main()
