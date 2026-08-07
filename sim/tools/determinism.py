#!/usr/bin/env python3
"""Run one render command N times, group the results by md5, and quantify the spread.

  tools/determinism.py -n 12 -- ./build/gpu_walk --size 640x360 --warm 240 --yaw 190

The command must accept --out PATH; the tool injects it. Every state that appears gets one
representative PNG kept in the work dir. If more than one state appears, the two most frequent are
differenced pixel by pixel and the difference is reported as: share of pixels off by more than 2
codes, share off by more than 32, the maximum deviation, and the bounding rows/columns of the
affected region. Exit 0 means one single state, exit 1 means the run is not deterministic.
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import zlib


def png_rgba(path):
    """Minimal PNG reader: 8-bit truecolour(+alpha), no interlace. Returns (w, h, bytes RGBA)."""
    with open(path, "rb") as f:
        data = f.read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    pos, idat, w = 8, bytearray(), None
    while pos < len(data):
        ln = int.from_bytes(data[pos:pos + 4], "big")
        typ = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        if typ == b"IHDR":
            w = int.from_bytes(body[0:4], "big")
            h = int.from_bytes(body[4:8], "big")
            depth, colour, _, _, interlace = body[8], body[9], body[10], body[11], body[12]
            assert depth == 8 and colour in (2, 6) and interlace == 0, (depth, colour, interlace)
            chan = 4 if colour == 6 else 3
        elif typ == b"IDAT":
            idat += body
        elif typ == b"IEND":
            break
        pos += 12 + ln
    raw = zlib.decompress(bytes(idat))
    stride = w * chan
    out = bytearray(w * h * 4)
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        ft = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if ft == 1:
            for i in range(chan, stride):
                line[i] = (line[i] + line[i - chan]) & 0xFF
        elif ft == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ft == 3:
            for i in range(stride):
                a = line[i - chan] if i >= chan else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ft == 4:
            for i in range(stride):
                a = line[i - chan] if i >= chan else 0
                c = prev[i - chan] if i >= chan else 0
                b = prev[i]
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        if chan == 4:
            out[y * w * 4:(y + 1) * w * 4] = line
        else:
            for x in range(w):
                out[(y * w + x) * 4:(y * w + x) * 4 + 3] = line[x * 3:x * 3 + 3]
                out[(y * w + x) * 4 + 3] = 255
        prev = line
    return w, h, bytes(out)


def diff(a_path, b_path):
    wa, ha, a = png_rgba(a_path)
    wb, hb, b = png_rgba(b_path)
    if (wa, ha) != (wb, hb):
        return {"error": "size mismatch", "a": [wa, ha], "b": [wb, hb]}
    n = wa * ha
    gt2 = gt32 = 0
    mx = 0
    rmin, rmax, cmin, cmax = ha, -1, wa, -1
    rows = [0] * ha
    for i in range(n):
        o = i * 4
        d = max(abs(a[o] - b[o]), abs(a[o + 1] - b[o + 1]), abs(a[o + 2] - b[o + 2]))
        if d > 2:
            gt2 += 1
            y, x = divmod(i, wa)
            rows[y] += 1
            if y < rmin: rmin = y
            if y > rmax: rmax = y
            if x < cmin: cmin = x
            if x > cmax: cmax = x
            if d > 32:
                gt32 += 1
        if d > mx:
            mx = d
    return {"pixels": n, "gt2": gt2, "gt2_pct": 100.0 * gt2 / n, "gt32": gt32,
            "gt32_pct": 100.0 * gt32 / n, "max_code": mx,
            "rows": [rmin, rmax] if rmax >= 0 else None,
            "cols": [cmin, cmax] if cmax >= 0 else None,
            "row_hist": rows}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-n", "--runs", type=int, default=10)
    ap.add_argument("-d", "--dir", default="build/determinism")
    ap.add_argument("--keep-all", action="store_true", help="keep every PNG, not one per state")
    ap.add_argument("--log", action="store_true", help="keep stdout of every run")
    ap.add_argument("--against", help="also diff the most frequent state against this reference PNG")
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    args = ap.parse_args()
    cmd = args.cmd[1:] if args.cmd and args.cmd[0] == "--" else args.cmd
    if not cmd:
        ap.error("no command")
    os.makedirs(args.dir, exist_ok=True)

    def binhash():
        try:
            return hashlib.md5(open(cmd[0], "rb").read()).hexdigest()
        except OSError:
            return "?"

    states, order, runs, bins = {}, [], [], {}
    for i in range(args.runs):
        out = os.path.join(args.dir, "run%03d.png" % i)
        # The binary is measured per run, not once: a concurrent build swapping it under the
        # experiment is indistinguishable from nondeterminism unless it is recorded here.
        bh = binhash()
        bins.setdefault(bh, []).append(i)
        t0 = time.time()
        r = subprocess.run(cmd + ["--out", out], capture_output=True, text=True)
        dt = time.time() - t0
        if r.returncode != 0 or not os.path.exists(out):
            print("run %d FAILED rc=%d\n%s" % (i, r.returncode, r.stderr[-2000:]))
            continue
        h = hashlib.md5(open(out, "rb").read()).hexdigest()
        if h not in states:
            states[h] = []
            order.append(h)
            shutil.copyfile(out, os.path.join(args.dir, "state_%s.png" % h[:8]))
        states[h].append(i)
        runs.append({"i": i, "md5": h, "sec": round(dt, 2), "bin": bh})
        if args.log:
            open(os.path.join(args.dir, "run%03d.log" % i), "w").write(r.stdout + r.stderr)
        if not args.keep_all:
            os.remove(out)
        print("run %3d  %s  %5.1fs  state %d" % (i, h[:8], dt, order.index(h)), flush=True)

    print("\n%d runs, %d distinct states" % (len(runs), len(states)))
    for k, h in enumerate(order):
        print("  state %d  %s  n=%-3d  runs %s" % (k, h[:8], len(states[h]), states[h]))
    if len(bins) > 1:
        print("\n  THE BINARY MOVED DURING THE RUN — the states below are not one experiment:")
        for b, r in bins.items():
            print("    %s  runs %s" % (b[:8], r))

    report = {"cmd": cmd, "runs": runs, "binaries": {b: r for b, r in bins.items()},
              "states": [{"md5": h, "n": len(states[h]), "runs": states[h]} for h in order]}

    def show(tag, a, b):
        d = diff(a, b)
        print("\ndiff %s" % tag)
        print("  > 2 codes : %8d px  %6.2f %%" % (d["gt2"], d["gt2_pct"]))
        print("  > 32 codes: %8d px  %6.2f %%" % (d["gt32"], d["gt32_pct"]))
        print("  max       : %d codes" % d["max_code"])
        print("  rows      : %s   cols: %s" % (d["rows"], d["cols"]))
        return {"a": a, "b": b, **d}

    rank = sorted(order, key=lambda h: -len(states[h]))
    if len(order) > 1:
        report["diff"] = show("%s vs %s" % (rank[0][:8], rank[1][:8]),
                              os.path.join(args.dir, "state_%s.png" % rank[0][:8]),
                              os.path.join(args.dir, "state_%s.png" % rank[1][:8]))
    if args.against and rank:
        report["against"] = show("%s vs reference %s" % (rank[0][:8], args.against),
                                 os.path.join(args.dir, "state_%s.png" % rank[0][:8]), args.against)
    open(os.path.join(args.dir, "report.json"), "w").write(json.dumps(report, indent=1))
    print("\nreport: %s" % os.path.join(args.dir, "report.json"))
    return 0 if len(order) <= 1 else 1


if __name__ == "__main__":
    sys.exit(main())
