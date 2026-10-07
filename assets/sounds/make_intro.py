#!/usr/bin/env python3
"""Writes intro_generated.wav, a short tune for the start-up intro.

This was the first intro tune. The one in use now is intro.wav, chosen by
the project owner; this script no longer touches it. To use the generated
tune again, copy intro_generated.wav over intro.wav.

The tune is made here from plain sine waves, so it is the project's own and
can be changed by editing the notes below. Run it from this folder:

    python3 make_intro.py

The intro lasts as long as intro.wav does, so no code needs changing.
"""
import math
import struct
import wave

RATE = 22050            # samples a second; half the menu's rate keeps the ROM small
LENGTH = 2.6            # seconds

# (start in seconds, pitch in Hz, loudness, seconds for the note to die away)
NOTES = [
    (0.15, 293.66, 0.8, 0.35),   # D4   four rising notes...
    (0.33, 440.00, 0.8, 0.35),   # A4
    (0.51, 587.33, 0.8, 0.35),   # D5
    (0.69, 739.99, 0.8, 0.40),   # F#5
    (0.90, 880.00, 0.7, 0.60),   # A5   ...then a chord that rings out
    (0.90, 587.33, 0.5, 0.60),   # D5
    (0.90, 739.99, 0.5, 0.60),   # F#5
    (0.90, 146.83, 0.6, 0.70),   # D3   a low note under the chord
]

ATTACK = 0.008          # seconds for a note to reach full loudness (avoids a click)
FADE_OUT = 0.25         # the last part of the tune fades to silence


def note(t, start, pitch, loudness, decay):
    t -= start
    if t < 0:
        return 0.0
    envelope = min(t / ATTACK, 1.0) * math.exp(-t / decay)
    phase = 2 * math.pi * pitch * t
    tone = math.sin(phase) + 0.30 * math.sin(2 * phase) + 0.10 * math.sin(3 * phase)
    return loudness * envelope * tone


samples = []
for i in range(int(RATE * LENGTH)):
    t = i / RATE
    value = sum(note(t, *n) for n in NOTES)
    value *= min((LENGTH - t) / FADE_OUT, 1.0)
    samples.append(value)

peak = max(abs(s) for s in samples)
scale = 0.7 * 32767 / peak

with wave.open("intro_generated.wav", "wb") as out:
    out.setnchannels(1)
    out.setsampwidth(2)
    out.setframerate(RATE)
    out.writeframes(b"".join(struct.pack("<h", int(s * scale)) for s in samples))

print(f"intro_generated.wav: {len(samples)} samples, {LENGTH} s at {RATE} Hz")
