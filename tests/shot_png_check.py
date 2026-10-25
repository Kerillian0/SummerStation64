# Checks the pictures tests/shot_png_test.c wrote: each must be a valid PNG
# (chunk CRCs, zlib stream and its checksum) holding exactly its raw pixels.
# Python's standard library only. Run with tests/run.sh.
import glob, os, struct, sys, zlib

folder = sys.argv[1]
failures = checks = 0
sizes = {}
for png in sorted(glob.glob(os.path.join(folder, "shot_*.png"))):
    checks += 1
    data = open(png, "rb").read()
    raw = open(png[:-4] + ".raw", "rb").read()
    try:
        assert data[:8] == b"\x89PNG\r\n\x1a\n", "signature"
        at, idat, width = 8, b"", None
        while at < len(data):
            length, kind = struct.unpack(">I4s", data[at:at + 8])
            body = data[at + 8:at + 8 + length]
            crc, = struct.unpack(">I", data[at + 8 + length:at + 12 + length])
            assert zlib.crc32(kind + body) == crc, "chunk CRC"
            if kind == b"IHDR":
                width, height = struct.unpack(">II", body[:8])
                assert body[8:] == b"\x08\x02\x00\x00\x00", "header"
            elif kind == b"IDAT":
                idat += body
            at += 12 + length
        assert kind == b"IEND", "ends with IEND"
        pixels = zlib.decompress(idat)      # checks the Adler-32 too
        row = 1 + width * 3
        assert len(pixels) == row * height, "size"
        out = b"".join(pixels[y * row + 1:(y + 1) * row] for y in range(height))
        assert all(pixels[y * row] == 0 for y in range(height)), "filter bytes"
        assert out == raw, "pixels differ"
        sizes[os.path.basename(png)] = (len(data), len(raw))
    except AssertionError as e:
        failures += 1
        print("  FAIL: %s: %s" % (os.path.basename(png), e))
for name in ("shot_0_640x480.png", "shot_1_640x480.png", "shot_3_640x480.png", "shot_2_640x480.png"):
    if name in sizes:
        print("  %s: %d KB (%d KB unpacked)" % (name, sizes[name][0] // 1024, sizes[name][1] // 1024))
print("%d of %d pictures failed." % (failures, checks) if failures else "All %d pictures check out." % checks)
sys.exit(1 if failures else 0)
