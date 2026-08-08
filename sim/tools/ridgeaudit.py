#!/usr/bin/env python3
"""Silhouettenrauheit: mittlerer |dy/dx| der Himmelskante je Spalte, 320x180.

  tools/ridgeaudit.py --render build/gpu_walk --a /tmp/A
  tools/ridgeaudit.py --a /tmp/A --b /tmp/B

Die Kante ist die oberste Zeile je Spalte, in der der vertikale Luminanzsprung |L(y+1)-L(y-1)| einen
Schwellwert reisst. Der Himmel ist ein glatter Verlauf (unter 0,4 Codes je Zeile ueber 180 Zeilen),
eine Gelaendekante ist eine Stufe; k = 10 Anzeigecodes liegt eine Groessenordnung ueber dem Verlauf
und unter jeder Kante.

DIE B-R-REGEL AUS tools/swardaudit.py TAUGT HIER NICHT, und das ist gemessen: Dunst macht fernes
Gelaende blauer als die Schwelle, also findet sie nicht die Himmelskante sondern eine chromatische
Hoehenlinie MITTEN IM BILD -- am Hochkoenig im Talgrund. Was sie dort misst, ist die Farbe der
Bodenbedeckung und nicht die Silhouette.

WAS DIESE ZAHL AUF DEN SECHS PAAREN NICHT AUFLOEST: die Foto-Himmelskante ist an vier von sechs
Kameras Wolkenoberkante und nicht Grat (Hochkoenig, Nebelhorn, Zugspitze, Hochries), und der
Fernhorizont steht bei 30-80 km (gemessen aus dem Tiefenpuffer), wo ein Grat von 350 m EINEN Pixel
hoch ist. Die Zahl misst dort Dunst und Wolken, nicht Gelaendedichte -- sie bewegt sich zwischen
kGrid 32 und 128 um weniger als 0,05 px. Fuer das Nahfeld ist die Zahl blind, weil dort nichts an
den Himmel stoesst.
"""
import argparse, hashlib, json, pathlib, subprocess, sys

import numpy as np
from PIL import Image

SIM = pathlib.Path(__file__).resolve().parent.parent
CAMS = ["nebelhorn", "herzogstand", "innsbruck", "hochries", "zugspitze", "hochkoenig"]
W, H = 320, 180
YW = np.array([0.2126, 0.7152, 0.0722], np.float32)
K = 10.0


def rgb(path):
    return np.asarray(Image.open(path).convert("RGB").resize((W, H), Image.LANCZOS), np.float32)


def skyline(img):
    l = img @ YW
    l = np.pad(l, ((0, 0), (1, 1)), mode="edge")
    l = (l[:, :-2] + l[:, 1:-1] + l[:, 2:]) / 3.0
    g = np.abs(l[2:] - l[:-2])
    y = np.zeros(W, np.int32)
    for x in range(W):
        r = 1
        while r < H - 1 and g[r - 1, x] < K:
            r += 1
        y[x] = r
    return y


def roughness(y):
    ok = (y > 1) & (y < H - 2)
    d = np.abs(np.diff(y.astype(np.float64)))
    keep = ok[:-1] & ok[1:]
    return (float(d[keep].mean()) if keep.any() else 0.0), float(y.mean())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render")
    ap.add_argument("--a")
    ap.add_argument("--b")
    ap.add_argument("--only")
    ap.add_argument("--out", default=str(SIM / "web" / "cams"))
    a = ap.parse_args()
    cams = json.loads((SIM.parent / "mods/webcams/cams.json").read_text())["cams"]
    alt = {c["slug"]: c["altM"] for c in cams}
    want = a.only.split(",") if a.only else CAMS
    if a.render:
        print("bin md5", hashlib.md5(pathlib.Path(a.render).read_bytes()).hexdigest())
    sets = {}
    if a.a:
        sets["A"] = pathlib.Path(a.a)
    if a.b:
        sets["B"] = pathlib.Path(a.b)
    if not sets:
        sets["R"] = pathlib.Path(a.out)

    tot = {k: 0 for k in sets}
    for s in want:
        f, fy = roughness(skyline(rgb(SIM / "web" / "cams" / f"{s}-fit.jpg")))
        line = [f"{s:12s}", f"Foto {f:5.2f} (Zeile {fy:5.1f})"]
        for k, d in sets.items():
            png = d / f"{s}-fit.png"
            if a.render and not png.exists():
                d.mkdir(parents=True, exist_ok=True)
                scene = SIM / "web" / "cams" / f"{s}-fit-scene.json"
                subprocess.run([str(pathlib.Path(a.render).resolve()), "--scene", str(scene),
                                "--out", str(png), "--warm", "20000",
                                "--eye-asl", f"{float(alt[s]):.2f}", "--size", f"{W}x{H}"],
                               capture_output=True, text=True, cwd=str(SIM))
            r, ry = roughness(skyline(rgb(png)))
            line.append(f"{k} {r:5.2f} (Zeile {ry:5.1f})  Anteil {r / f if f > 0 else 0:4.2f}")
            tot[k] += 1 if (f > 0 and r >= 0.5 * f) else 0
        print("  ".join(line))
    for k, hit in tot.items():
        print(f"{k}: {hit} von {len(want)} Kameras erreichen die halbe Fotorauheit")
    return 0


if __name__ == "__main__":
    sys.exit(main())
