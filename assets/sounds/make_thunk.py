#!/usr/bin/env python3
"""Writes thunk.wav, the soft thunk of a cartridge going home when a game
starts (game_start.c).

A real N64 cartridge doesn't click: it slides into the slot and stops with a
dull knock. This is made from scratch here, so it is the project's own: a
low tone that drops in pitch as it dies away (the body), a little muffled
noise at the start (the plastic meeting the slot), and a faint higher tone
that dies quicker (the shell). Run it from this folder:

    python3 make_thunk.py
"""
import math
import random
import struct
import wave

RATE = 22050            # samples a second, as the menu's other sounds
LENGTH = 0.30           # seconds
PEAK = 0.75             # loudest sample, against full scale

# The body: a low tone sweeping down, dying away.
BODY_FROM, BODY_TO = 150.0, 58.0    # Hz
BODY_SWEEP = 0.09                   # seconds for the pitch to fall
BODY_DECAY = 0.07                   # seconds for it to fall to about a third
# The shell: a faint higher tone, gone quicker.
SHELL_PITCH = 240.0
SHELL_LOUDNESS = 0.25
SHELL_DECAY = 0.03
# The knock: a burst of noise with the highs taken off (muffled).
KNOCK_LOUDNESS = 0.35
KNOCK_DECAY = 0.012
KNOCK_SOFTNESS = 0.08               # 0..1: lower takes off more of the highs

ATTACK = 0.003                      # seconds to full loudness (no click at the start)


def main():
    random.seed(64)
    count = int(RATE * LENGTH)
    samples = []
    phase_body = 0.0
    phase_shell = 0.0
    smoothed = 0.0
    for i in range(count):
        t = i / RATE
        attack = min(1.0, t / ATTACK)
        sweep = min(1.0, t / BODY_SWEEP)
        pitch = BODY_FROM + (BODY_TO - BODY_FROM) * (1.0 - (1.0 - sweep) ** 2)
        phase_body += 2.0 * math.pi * pitch / RATE
        phase_shell += 2.0 * math.pi * SHELL_PITCH / RATE
        body = math.sin(phase_body) * math.exp(-t / BODY_DECAY)
        shell = SHELL_LOUDNESS * math.sin(phase_shell) * math.exp(-t / SHELL_DECAY)
        smoothed += KNOCK_SOFTNESS * (random.uniform(-1.0, 1.0) - smoothed)
        knock = KNOCK_LOUDNESS * smoothed * 4.0 * math.exp(-t / KNOCK_DECAY)
        fade = min(1.0, (LENGTH - t) / 0.03)        # the very end to silence
        samples.append(attack * fade * (body + shell + knock))

    loudest = max(abs(s) for s in samples) or 1.0
    with wave.open("thunk.wav", "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(b"".join(struct.pack("<h", int(s / loudest * PEAK * 32767)) for s in samples))
    print("thunk.wav: %d samples, %.2f s" % (count, LENGTH))


if __name__ == "__main__":
    main()
