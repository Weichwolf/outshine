#!/usr/bin/env python3
"""Is there a real world on this screenshot, or an empty sky?

Reads a PNG with the stdlib only — this box has no PIL, and adding a dependency to a check that
exists to keep the build honest would be its own joke.

Deliberately NOT a pixel-exact compare. The scene depends on live tiles, live weather and the
sun's actual position, so exact pixels are not a stable oracle; a golden image would fail every
sunset and teach everyone to ignore it. What IS stable is the property: real tiles reached the
screen and became ground.

  pngstat.py shot.png [label]      -> prints stats, exits 0 if the scene looks real
"""
import sys, zlib, struct


def read_png(path):
    """-> (w, h, bytes-per-pixel, pixel bytes). Handles the 8-bit RGB/RGBA PNGs chrome writes."""
    d = open(path, 'rb').read()
    if d[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError("not a PNG")
    w = h = ct = None
    idat = b''
    i = 8
    while i < len(d):
        ln = struct.unpack('>I', d[i:i+4])[0]
        typ = d[i+4:i+8]
        body = d[i+8:i+8+ln]
        if typ == b'IHDR':
            w, h, bd, ct = struct.unpack('>IIBB', body[:10])
            if bd != 8 or ct not in (2, 6):
                raise ValueError("expected 8-bit RGB/RGBA, got depth=%d colour=%d" % (bd, ct))
        elif typ == b'IDAT':
            idat += body
        elif typ == b'IEND':
            break
        i += 12 + ln
    raw = zlib.decompress(idat)
    bpp = 4 if ct == 6 else 3
    stride = w * bpp
    out = bytearray()
    prev = bytearray(stride)
    p = 0
    for _ in range(h):
        f = raw[p]; p += 1
        line = bytearray(raw[p:p+stride]); p += stride
        if f:
            for x in range(stride):
                a = line[x-bpp] if x >= bpp else 0
                b = prev[x]
                c = prev[x-bpp] if x >= bpp else 0
                if f == 1:   line[x] = (line[x] + a) & 255
                elif f == 2: line[x] = (line[x] + b) & 255
                elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
                elif f == 4:
                    pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                    pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                    line[x] = (line[x] + pr) & 255
        out += line
        prev = line
    return w, h, bpp, out


def is_ground(r, g, b):
    """Is this pixel ground rather than sky?

    The predicate this replaces was `not (b > r+20 and b > 90)` -- "anything not strongly blue is
    ground". That let three things through that are not ground: the pale haze along the horizon
    (near-white, b ~= r), the dark page title bar, and the HUD text. A screenshot of pure empty sky
    scored 10% "non-sky" and the gate was `> 10`, so an empty world passed as
    "OK — real terrain on screen". It did, for real, over two runs.

    What actually separates them: sky is blue-dominant (b > g) at every brightness, and so is its
    haze -- washing out moves it towards white, i.e. b -> g, never past it. Ground is the opposite:
    vegetation and fields are green- or earth-dominant (g > b, r > b). So require the pixel to beat
    blue by a margin. Neutral greys (title bar, cloud, HUD glyph edges) sit at b == g and are
    excluded rather than counted, which is the whole point.

    Grey asphalt is ground and is NOT counted here. That is deliberate: this is a floor on "did
    real tiles reach the screen", not a land-cover census, and a predicate that admits neutral grey
    would admit the title bar again.
    """
    return g > b + 8 or r > b + 8


def main():
    path = sys.argv[1]
    label = sys.argv[2] if len(sys.argv) > 2 else ''
    w, h, bpp, px = read_png(path)

    cols = set()
    ground = 0
    total = 0
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            o = (y * w + x) * bpp
            r, g, b = px[o], px[o+1], px[o+2]
            cols.add((r, g, b))
            total += 1
            if is_ground(r, g, b):
                ground += 1
    pct = 100.0 * ground / total

    # >200 colours: a flat error screen has a handful. 8..98% ground: some ground AND some sky --
    # all-ground means the camera is buried, all-sky means nothing streamed. 8% is low because a
    # level 500 m cruise legitimately puts the horizon high; it is a floor against "nothing", not
    # a measure of framing.
    ok = len(cols) > 200 and 8 < pct < 98
    print("  %-6s %dx%d, %d distinct colours, %.0f%% ground" % (label, w, h, len(cols), pct))
    print("  %-6s %s" % (label, "OK — real terrain on screen" if ok
                         else "SUSPECT — scene looks empty or degenerate"))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
