#!/usr/bin/env python3
"""Is there a real world on this screenshot, or an empty sky?

Reads a PNG with the stdlib only — this box has no PIL, and adding a dependency to a check that
exists to keep the build honest would be its own joke.

Deliberately NOT a pixel-exact compare. The scene depends on live tiles, live weather and the
sun's actual position, so exact pixels are not a stable oracle; a golden image would fail every
sunset and teach everyone to ignore it. What IS stable is the property: real tiles reached the
screen and became ground.

  pngstat.py shot.png [label]      -> prints stats, exits 0 if the scene looks real
  pngstat.py --selftest            -> check the predicate against the two fixtures, no stack needed
"""
import sys, zlib, struct, os


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

    The MARGIN IS A FRACTION OF THE PIXEL'S OWN BRIGHTNESS, not a constant. The constant `8` was
    the same daylight assumption as the predicate it replaced, one layer down: the sign survives
    darkness but the margin does not. `uLight = 0.20 + 0.80*day`, so at night the whole scene is
    dimmed to 20 % and a daylight g-b of ~25 arrives as ~5 -- under a threshold picked at noon.
    Measured on a real, fully streamed night scene (SUN -14, terrain/river/roads/stars all drawn):
    ground r=15.8 g=18.5 b=13.1, g-b = +5; sky g-b = -13. The sign was still perfectly correct;
    only the margin had shrunk with the light. So `g > b+8` scored that world 1.1 % ground --
    against 1.0 % for a screenshot with NO WORLD IN IT AT ALL. Not "less accurate at night": no
    discriminating power at night, and verify.sh's render gate was red every night from sunset to
    sunrise for a reason that had nothing to do with the renderer.

    Scaling the margin restores it: the same two fixtures score 33.0 % and 1.1 %.

    Why a fraction and not a bigger constant: FRAC*mean is 7.5 at daylight brightness (mean ~150),
    i.e. within half a level of the old 8. On the one genuinely daylight fixture available when
    this was written -- an empty world lit by the default sun -- old and new agree to 0.1
    percentage points (1.0 % vs 1.1 %). The daylight calibration is therefore preserved by
    construction, which mattered because at the time of the change the sun was down and the
    daylight case could not be re-measured, only left alone.

    STILL A COLOUR PREDICATE, so still an assumption about the landscape: rock and snow are
    neutral and are NOT counted. Over an alpine origin this will under-report and can still call a
    real world empty. Fixing that needs a structure criterion ("has detail no sky has") and a real
    rock screenshot to check it against -- not a third recalibration of the same idea.

    Grey asphalt is ground and is NOT counted here. That is deliberate: this is a floor on "did
    real tiles reach the screen", not a land-cover census, and a predicate that admits neutral grey
    would admit the title bar again.
    """
    # Floor: near-black pixels have no reliable chroma, and a pure ratio would let their noise in.
    need = max(2.0, 0.05 * (r + g + b) / 3.0)
    return (g - b) > need or (r - b) > need


def selftest():
    """Watch the checker succeed AND fail, on real screenshots, without a stack or a time of day.

    These two fixtures exist because the night bug could only be caught at night. The predicate
    was wrong from sunset to sunrise for hours and every check we had said nothing, because the
    checks all ran in daylight -- and a daytime session cannot reproduce a night scene on demand:
    the sun is an input we do not control. So the scene is kept, not the conditions that made it.

    day-empty-world.png is the falsification half. A checker that only ever proves it says OK to
    good pictures has not been tested at all -- pngstat's own history is two runs of "OK — real
    terrain on screen" over an empty sky.
    """
    here = os.path.dirname(os.path.abspath(__file__))
    cases = [('night-real-world.png', True,
              'a real, fully streamed night scene (SUN -14) — the case that was red every night'),
             ('day-empty-world.png', False,
              'no world at all, daylight sky and haze — must never pass')]
    bad = 0
    for name, want_ok, why in cases:
        p = os.path.join(here, 'fixtures', name)
        if not os.path.exists(p):
            print("  selftest: missing fixture %s" % p); return 1
        w, h, bpp, px = read_png(p)
        got_ok, pct = verdict(w, h, bpp, px)
        mark = 'ok  ' if got_ok == want_ok else 'FAIL'
        if got_ok != want_ok:
            bad += 1
        print("  %s %-22s %5.1f%% ground, expected %-7s %s"
              % (mark, name, pct, 'OK' if want_ok else 'SUSPECT', why))
    print("  selftest: %s" % ("passed" if not bad else "%d FAILED" % bad))
    return 1 if bad else 0


def verdict(w, h, bpp, px):
    """-> (ok, ground_pct). Shared by main() and the selftest so they cannot drift apart."""
    cols = set()
    ground = total = 0
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            o = (y * w + x) * bpp
            r, g, b = px[o], px[o+1], px[o+2]
            cols.add((r, g, b))
            total += 1
            if is_ground(r, g, b):
                ground += 1
    pct = 100.0 * ground / total
    return (len(cols) > 200 and 8 < pct < 98), pct


def main():
    if sys.argv[1] == '--selftest':
        return selftest()
    path = sys.argv[1]
    label = sys.argv[2] if len(sys.argv) > 2 else ''
    w, h, bpp, px = read_png(path)
    # One scoring path, shared with --selftest. A checker whose self-test scores differently from
    # its gate is render_native.c again: the thing calling itself the check checking a scene
    # nobody runs.
    ok, pct = verdict(w, h, bpp, px)
    cols = set((px[(y*w+x)*bpp], px[(y*w+x)*bpp+1], px[(y*w+x)*bpp+2])
               for y in range(0, h, 2) for x in range(0, w, 2))
    print("  %-6s %dx%d, %d distinct colours, %.0f%% ground" % (label, w, h, len(cols), pct))
    print("  %-6s %s" % (label, "OK — real terrain on screen" if ok
                         else "SUSPECT — scene looks empty or degenerate"))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
