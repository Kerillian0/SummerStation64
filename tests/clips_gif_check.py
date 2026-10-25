# Decodes the GIFs tests/clips_tab_test.js wrote, with Pillow, and compares
# every frame with the pixels that went in. Run with tests/run.sh.
import os, sys
from PIL import Image, ImageSequence

folder = sys.argv[1]
failures = 0
for name, size, frames in (("moving", (64, 48), 4), ("noise", (320, 240), 1)):
    im = Image.open(os.path.join(folder, name + ".gif"))
    raw = open(os.path.join(folder, name + ".raw"), "rb").read()
    got = [f.convert("RGB").tobytes() for f in ImageSequence.Iterator(im)]
    if im.size != size:
        failures += 1; print("  FAIL: %s is %s" % (name, im.size))
    # "moving" has five frames in, one unchanged: four come out, the repeat merged into the one before.
    want = [raw[i * size[0] * size[1] * 3:(i + 1) * size[0] * size[1] * 3] for i in range(len(raw) // (size[0] * size[1] * 3))]
    if name == "moving":
        want = [want[0], want[1], want[3], want[4]]
    if len(got) != frames:
        failures += 1; print("  FAIL: %s has %d frames, not %d" % (name, len(got), frames))
    for i, (g, w) in enumerate(zip(got, want)):
        if g != w:
            failures += 1; print("  FAIL: %s frame %d differs" % (name, i + 1))
    if name == "moving" and im.info.get("loop") != 0:
        failures += 1; print("  FAIL: moving.gif doesn't loop")
print("%d failed." % failures if failures else "Both GIFs decode to exactly what went in.")
sys.exit(1 if failures else 0)
