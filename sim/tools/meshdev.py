#!/usr/bin/env python3
"""Was das gezeichnete Netz vom Quellgitter wegwirft, in Metern.

  tools/meshdev.py --grid 96
  tools/meshdev.py --grid 96,128,192 --only hochkoenig

Eine z14-Terrarium-Kachel traegt 256x256 Stuetzstellen. `ChunkBuildEcef` (render/ChunkMesh.h) waehlt
davon (kGrid+1)^2 Postings ueber `W3_CI(i) = i*(C-1)/(gc-1)` und spannt zwischen ihnen zwei Dreiecke
je Quad. Diese Datei laeuft genau denselben Fehlergang: jeder QUELLTEXEL wird gegen die gezeichnete
Flaeche an seiner Stelle gehalten. max ist die Zahl, die `Chunk.err` traegt und den LOD-Fehler setzt;
RMS ist, was die Silhouette im Mittel verliert.

Das Kachelfeld ist 3x3 um die Kamerakachel, weil ein einzelnes Quadrat je nach Kachelgrenze eine
Wand oder ein Talboden sein kann.
"""
import argparse
import io
import json
import math
import pathlib
import sys as _sys
_sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import webcammod
import sys
import urllib.request

import numpy as np
from PIL import Image

SIM = pathlib.Path(__file__).resolve().parent.parent
CAMS = ["nebelhorn", "herzogstand", "innsbruck", "hochries", "zugspitze", "hochkoenig"]
BASE = "http://localhost:8081"


def tile_xy(lat, lon, z):
    n = 2 ** z
    x = int((lon + 180.0) / 360.0 * n)
    y = int((1.0 - math.log(math.tan(math.radians(lat)) + 1.0 / math.cos(math.radians(lat))) / math.pi) / 2.0 * n)
    return x, y


def dem(z, x, y):
    with urllib.request.urlopen(f"{BASE}/t/terrain/{z}/{x}/{y}", timeout=30) as r:
        px = np.asarray(Image.open(io.BytesIO(r.read())).convert("RGB"), np.float64)
    return px[:, :, 0] * 256.0 + px[:, :, 1] + px[:, :, 2] / 256.0 - 32768.0


def cell(idx, n):
    """Je Quellindex 0..n-1: die Zelle j mit idx[j] <= r <= idx[j+1] und der Bruchteil darin."""
    r = np.arange(n)
    j = np.clip(np.searchsorted(idx, r, "right") - 1, 0, len(idx) - 2)
    span = (idx[j + 1] - idx[j]).astype(np.float64)
    return j, (r - idx[j]) / np.where(span > 0, span, 1.0)


def deviation(h, grid):
    """h: (R,C) Quellhoehen. Rueckgabe: (max, Quadratsumme, n) der Abweichung je Quelltexel."""
    R, C = h.shape
    gr, gc = min(R, grid + 1), min(C, grid + 1)
    ri = (np.arange(gr, dtype=np.int64) * (R - 1)) // (gr - 1)
    ci = (np.arange(gc, dtype=np.int64) * (C - 1)) // (gc - 1)
    nh = h[np.ix_(ri, ci)]
    jr, sv = cell(ri, R)
    jc, su = cell(ci, C)
    J, I = jr[:, None], jc[None, :]
    V, U = sv[:, None], su[None, :]
    h00, h10 = nh[J, I], nh[J, I + 1]
    h01, h11 = nh[J + 1, I], nh[J + 1, I + 1]
    lo = h00 + (h10 - h00) * U + (h11 - h10) * V
    up = h00 + (h11 - h01) * U + (h01 - h00) * V
    d = np.abs(np.where(U >= V, lo, up) - h)
    return float(d.max()), float((d * d).sum()), d.size


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--grid", default="96")
    ap.add_argument("--only")
    ap.add_argument("--z", type=int, default=14)
    ap.add_argument("--ring", type=int, default=1)
    a = ap.parse_args()
    cams = {c["slug"]: c for c in webcammod.cams()}
    want = a.only.split(",") if a.only else CAMS
    grids = [int(g) for g in a.grid.split(",")]

    tiles = {}
    for s in want:
        x0, y0 = tile_xy(cams[s]["lat"], cams[s]["lon"], a.z)
        tiles[s] = [(x0 + dx, y0 + dy) for dy in range(-a.ring, a.ring + 1)
                    for dx in range(-a.ring, a.ring + 1)]

    print(f"z{a.z}, {(2 * a.ring + 1)**2} Kacheln je Kamera, Fehler je Quelltexel gegen die gezeichnete Flaeche")
    print(f"{'Kamera':13s}" + "".join(f"  kGrid {g:<3d} max/RMS m" % () for g in grids))
    for s in want:
        hs = [dem(a.z, x, y) for x, y in tiles[s]]
        line = f"{s:13s}"
        for g in grids:
            mx, ssq, n = 0.0, 0.0, 0
            for h in hs:
                m2, s2, n2 = deviation(h, g)
                mx = max(mx, m2)
                ssq += s2
                n += n2
            line += f"  {mx:8.2f} / {math.sqrt(ssq / n):5.2f}"
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
