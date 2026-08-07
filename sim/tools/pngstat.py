#!/usr/bin/env python3
"""Mittlere Rec.709-Luminanz ueber rechteckige Bildregionen eines PNG.

  tools/pngstat.py bild.png x,y,w,h[:name] ...
  tools/pngstat.py bild.png --grid 8x6
"""
import struct, sys, zlib


def read_rgb(path):
    d = open(path, "rb").read()
    i, w, h, ct, idat = 8, 0, 0, 6, b""
    while i < len(d):
        ln = struct.unpack(">I", d[i:i + 4])[0]
        t = d[i + 4:i + 8]
        if t == b"IHDR":
            w, h, _bd, ct = struct.unpack(">IIBB", d[i + 8:i + 18])
        elif t == b"IDAT":
            idat += d[i + 8:i + 8 + ln]
        i += 12 + ln
    raw = zlib.decompress(idat)
    bpp = 4 if ct == 6 else 3
    stride = w * bpp
    out = bytearray()
    prev = bytearray(stride)
    o = 0
    for _y in range(h):
        f = raw[o]
        o += 1
        line = bytearray(raw[o:o + stride])
        o += stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        out += line
        prev = line
    return w, h, bpp, out


def region(w, _h, bpp, px, x0, y0, rw, rh):
    s = n = 0.0
    rs = gs = bs = 0.0
    for y in range(y0, y0 + rh):
        base = y * w * bpp
        for x in range(x0, x0 + rw):
            o = base + x * bpp
            r, g, b = px[o], px[o + 1], px[o + 2]
            s += 0.2126 * r + 0.7152 * g + 0.0722 * b
            rs += r
            gs += g
            bs += b
            n += 1
    return s / n, rs / n, gs / n, bs / n


def main():
    path = sys.argv[1]
    w, h, bpp, px = read_rgb(path)
    specs = []
    args = sys.argv[2:]
    if args and args[0] == "--grid":
        gx, gy = (int(v) for v in args[1].split("x"))
        cw, ch = w // gx, h // gy
        for j in range(gy):
            for k in range(gx):
                specs.append((k * cw, j * ch, cw, ch, f"g{k}{j}"))
    else:
        for a in args:
            body, _, name = a.partition(":")
            x, y, rw, rh = (int(v) for v in body.split(","))
            specs.append((x, y, rw, rh, name or body))
    vals = []
    for x, y, rw, rh, name in specs:
        lum, r, g, b = region(w, h, bpp, px, x, y, rw, rh)
        vals.append(lum)
        print(f"{name:16s} {x:4d},{y:4d} {rw:3d}x{rh:3d}  Y={lum:7.2f}  rgb={r:6.1f},{g:6.1f},{b:6.1f}")
    if len(vals) > 1:
        lo, hi = min(vals), max(vals)
        print(f"{'SPANNE':16s} min={lo:.2f} max={hi:.2f} delta={hi - lo:.2f} "
              f"rel={(hi - lo) / max(hi, 1e-9) * 100:.1f}%")


if __name__ == "__main__":
    main()
