#!/usr/bin/env python3
"""RetroAchievements experiment, step 2a: the PC chooses what the game copies.

With the hidden option `ra_test = 1` in sd:/menu/options.ini and Super Mario
64 (USA) started on a console with the Expansion Pak, the menu's hook runs a
small copy loop in the game once a frame (src/boot/ra_hook.h). This script
tells it what to copy (Mario's coins, stars, lives and health, as two ranges),
then reads the ring of the last 16 frames a few times a second and goes
through every frame in order, as an achievement checker must: it prints a
line whenever a value changes, with the frame it changed on, and counts any
frames it missed.

    pip install pyserial
    python ra_pipe_test.py            (finds the SC64 by itself)
    python ra_pipe_test.py --port COM5

Close sc64deployer (and anything else using the cart's USB port) first.
Ctrl+C stops it. It writes only the copy list, in the cart's spare memory.

Protocol (SummerCart64 docs/03_usb_interface.md): a command is "CMD", one ID
character, two big-endian uint32 arguments, then any data; the answer is
"CMP" (or "ERR"), the same ID, a big-endian uint32 length, then the data.
'v' answers "SCv2"; 'm' MEMORY_READ (address, length); 'M' MEMORY_WRITE
(address, length, data). The cart's memory starts at 0 from USB; the N64 sees
it 0x10000000 higher.
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

# src/boot/ra_hook.h, as seen from USB
LIST, STATUS, RING = 0x03B00000, 0x03B01000, 0x03B10000
SLOTS, SLOT_SIZE = 16, 4096

# Super Mario 64 (USA): Mario's state (the addresses the well-known GameShark codes use), as RDRAM offsets
RANGES = [(0x0033B218, 4),      # coins (s16), stars (s16)
          (0x0033B21C, 4)]      # (a byte), lives (s8), health (s16: wedges in the high byte)


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


def read_exact(link, count, seconds=2.0):
    data = b""
    end = time.time() + seconds
    while len(data) < count:
        piece = link.read(count - len(data))
        if piece:
            data += piece
        elif time.time() > end:
            raise TimeoutError(f"waited for {count} bytes, got {len(data)}")
    return data


def answer(link, cmd_id, seconds=2.0):
    while True:
        head = read_exact(link, 4, seconds)
        length = struct.unpack(">I", read_exact(link, 4))[0]
        payload = read_exact(link, length) if length else b""
        if head[:3] == b"ERR":
            raise RuntimeError(f"the cart answered ERR to '{cmd_id.decode()}'")
        if head[:3] == b"CMP" and head[3:4] == cmd_id:
            return payload
        # anything else (debug packets, a late answer to another command) is not ours


def command(link, cmd_id, arg0=0, arg1=0, data=b""):
    link.write(b"CMD" + cmd_id + struct.pack(">II", arg0, arg1) + data)
    return answer(link, cmd_id)


def write_memory(link, address, data):
    link.write(b"CMD" + b"M" + struct.pack(">II", address, len(data)) + data)
    try:
        answer(link, b"M", seconds=0.5)     # the docs list no answer data; take a CMP if one comes
    except TimeoutError:
        pass


def set_list(link, ranges):
    body = struct.pack(">I", len(ranges)) + b"".join(struct.pack(">II", a, n) for a, n in ranges)
    write_memory(link, LIST, b"\0\0\0\0" + body)    # not valid while it changes ...
    write_memory(link, LIST, b"RAL2")                # ... until "RAL2" goes in, last


def decode(data):
    coins = int.from_bytes(data[0:2], "big", signed=True)
    stars = int.from_bytes(data[2:4], "big", signed=True)
    lives = int.from_bytes(data[5:6], "big", signed=True)
    health = int.from_bytes(data[6:8], "big", signed=True) >> 8
    return {"coins": coins, "stars": stars, "lives": lives, "health": health}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--port", help="the SC64's serial port (COM5, /dev/ttyUSB0, ...)")
    parser.add_argument("--every", type=float, default=0.1, help="seconds between reads (default 0.1; the ring holds 16 frames)")
    args = parser.parse_args()

    port = args.port or find_port()
    print(f"Opening {port}...")
    link = serial.Serial(port, timeout=0.5, write_timeout=2.0)
    reset(link)
    print(f"Cart says {command(link, b'v')!r} (expected b'SCv2')")
    set_list(link, RANGES)
    size = sum(n for _, n in RANGES)
    print(f"Asked for {len(RANGES)} ranges, {size} bytes a frame. Start Super Mario 64 (USA) if it isn't running yet. Ctrl+C stops.\n")

    last_frame = None
    last_values = None
    processed = missed = 0
    started = time.time()
    try:
        while True:
            status = command(link, b"m", STATUS, 8)
            if status[:4] != b"RAH3":
                print(f"\rNo hook yet (status holds {status[:4]!r}).        ", end="", flush=True)
                time.sleep(0.5)
                continue
            now_frame = int.from_bytes(status[4:8], "big")
            ring = command(link, b"m", RING, SLOTS * SLOT_SIZE)
            frames = {}
            for slot in range(SLOTS):
                base = slot * SLOT_SIZE
                frame = int.from_bytes(ring[base:base + 4], "big")
                # only frames the hook can't have started overwriting while we read
                if now_frame - (SLOTS - 2) <= frame <= now_frame and frame % SLOTS == slot:
                    frames[frame] = ring[base + 4:base + 4 + size]
            for frame in sorted(frames):
                if last_frame is not None and frame <= last_frame:
                    continue
                if last_frame is not None and frame > last_frame + 1:
                    missed += frame - last_frame - 1
                values = decode(frames[frame])
                if last_values is not None and values != last_values:
                    changes = ", ".join(f"{k} {last_values[k]} -> {v}" for k, v in values.items() if v != last_values[k])
                    print(f"\rframe {frame:8d}: {changes}{' ' * 30}")
                last_values = values
                last_frame = frame
                processed += 1
            if last_values is not None:
                rate = processed / max(time.time() - started, 0.001)
                print(f"\rframe {last_frame:8d}  {rate:5.1f} frames/s checked, {missed} missed  |  coins {last_values['coins']:3d}  "
                      f"stars {last_values['stars']:3d}  lives {last_values['lives']:2d}  health {last_values['health']}/8   ",
                      end="", flush=True)
            time.sleep(args.every)
    except KeyboardInterrupt:
        print(f"\nStopped: {processed} frames checked, {missed} missed.")
    finally:
        link.close()


if __name__ == "__main__":
    main()
