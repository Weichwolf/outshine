#!/usr/bin/env python3
"""Abstand zum Foto auf EINGEFRORENEN Masken, Render gegen web/cams/<slug>-fit.jpg.

  tools/swardaudit.py --render build/gpu_walk
  tools/swardaudit.py --a /tmp/before --b /tmp/after      # zwei Bildsaetze vergleichen

Die Maske entsteht EINMAL aus dem Foto und wird als <slug>-mask.png abgelegt; ab dann bewegt sie
sich nicht mehr. Eine Maske, die aus dem RENDER kommt, wandert mit jeder Aenderung und misst dann
sich selbst -- deshalb ist die Referenz das Foto, das nie wieder anders aussieht.

Zwei Masken je Kamera:
  boden  jede Zeile unterhalb der Horizontlinie des Fotos (die Kontrolle)
  narbe  davon die Pixel, die das FOTO als Vegetation fuehrt (G groesster Kanal und G-B > 8)

Zwei Zahlen je Maske, beide in Anzeigecodes auf 320x180:
  |dL|      Betrag des Unterschieds der mittleren Luminanz
  |dSigma|  Betrag des Unterschieds der Luminanz-Standardabweichung
"""
import argparse, hashlib, json, os, pathlib, subprocess, sys

import numpy as np
from PIL import Image

SIM = pathlib.Path(__file__).resolve().parent.parent
CAMS = ["nebelhorn", "herzogstand", "innsbruck", "hochries", "zugspitze", "hochkoenig"]
W, H = 320, 180
YW = np.array([0.2126, 0.7152, 0.0722], np.float32)


def rgb(path):
    return np.asarray(Image.open(path).convert("RGB").resize((W, H), Image.LANCZOS), np.float32)


def lum(a):
    return a @ YW


def masks(photo):
    """Horizont je Spalte: der unterste Lauf von oben, in dem B-R > 6 gilt. Ein Himmel ist blauer
    als jeder Boden; wo das Foto keinen blauen Himmel hat (Hochnebel), liefert die Regel Zeile 0
    und die Maske ist das ganze Bild, was fuer eine Bodenmessung die konservative Seite ist."""
    br = photo[:, :, 2] - photo[:, :, 0]
    ground = np.zeros((H, W), bool)
    for x in range(W):
        y = 0
        while y < H and br[y, x] > 6.0:
            y += 1
        ground[y:, x] = True
    g = photo[:, :, 1]
    veg = ground & (g >= photo[:, :, 0]) & (g >= photo[:, :, 2]) & ((g - photo[:, :, 2]) > 8.0)
    return {"boden": ground, "narbe": veg}


def stats(img, m):
    l = lum(img)[m]
    return float(l.mean()), float(l.std())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render")
    ap.add_argument("--a")
    ap.add_argument("--b")
    ap.add_argument("--only")
    ap.add_argument("--out", default=str(SIM / "web" / "cams"))
    a = ap.parse_args()
    want = a.only.split(",") if a.only else CAMS
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    for p in (a.render, ):
        if p:
            print("bin md5", hashlib.md5(pathlib.Path(p).read_bytes()).hexdigest())

    sets = {}
    if a.a:
        sets["A"] = pathlib.Path(a.a)
    if a.b:
        sets["B"] = pathlib.Path(a.b)
    if not sets:
        sets["R"] = out

    win = {k: 0 for k in sets}
    rows = []
    for s in want:
        photo = rgb(SIM / "web" / "cams" / f"{s}-fit.jpg")
        mk = masks(photo)
        mp = out / f"{s}-mask.png"
        vis = np.zeros((H, W, 3), np.uint8)
        vis[mk["boden"]] = (60, 60, 60)
        vis[mk["narbe"]] = (0, 220, 0)
        Image.fromarray(vis).save(mp)
        for name, m in mk.items():
            pm, ps = stats(photo, m)
            line = [s, name, f"n={int(m.sum()):6d}", f"F {pm:6.1f}/{ps:5.1f}"]
            vals = {}
            for k, d in sets.items():
                png = d / f"{s}-fit.png"
                if a.render and not png.exists():
                    subprocess.run([str(pathlib.Path(a.render).resolve()), "webcams", f"{s}-fit"],
                                   capture_output=True, text=True, cwd=str(SIM),
                                   env=dict(os.environ, OUTSHINE_OUT=str(d)))
                rm, rs = stats(rgb(png), m)
                vals[k] = (abs(rm - pm), abs(rs - ps))
                line.append(f"{k} {rm:6.1f}/{rs:5.1f}  |dL| {vals[k][0]:5.2f} |dS| {vals[k][1]:5.2f}")
            if len(sets) == 2 and name == "narbe":
                dl = {k: v[0] for k, v in vals.items()}
                ds = {k: v[1] for k, v in vals.items()}
                better = (dl["B"] < dl["A"]) and (ds["B"] < ds["A"])
                line.append("BESSER" if better else
                            ("teils" if (dl["B"] < dl["A"]) or (ds["B"] < ds["A"]) else "schlechter"))
                win["B"] += 1 if better else 0
            rows.append(line)
            print("  ".join(line))
    if len(sets) == 2:
        print(f"\nnarbe: {win['B']} von {len(want)} Kameras in BEIDEN Zahlen naeher am Foto")
    return 0


if __name__ == "__main__":
    sys.exit(main())
