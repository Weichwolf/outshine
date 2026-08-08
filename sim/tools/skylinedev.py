#!/usr/bin/env python3
"""Die Himmelskante als ZEILE je Spalte, in voller Aufloesung.

  tools/skylinedev.py --render build/gpu_walk --out /tmp/A     # Bild + Tiefe je Kamera
  tools/skylinedev.py --a /tmp/A --b /tmp/B                    # Bau gegen Bau (aus der TIEFE)
  tools/skylinedev.py --a /tmp/A --calib                       # was der Luminanzdetektor kann
  tools/skylinedev.py --a /tmp/A --photo                       # Bau gegen Foto

WARUM DIESE ZAHL HAELT -- vier Punkte, und jeder ist gemessen statt behauptet.

1. IM BAU IST DIE KANTE EXAKT. Der Tiefenpuffer ist umgekehrtes Z: Himmel ist 0, Gelaende ist es
   nicht. Die oberste Zeile je Spalte mit d > 0 IST die Silhouette -- kein Schwellwert, kein Kontrast,
   also auch kein Dunst. Bau gegen Bau wird deshalb NIE ueber Pixel gemessen.

2. DUNST. Fuer das Foto gibt es keine Tiefe, also braucht es dort einen Bilddetektor -- und der wird
   am Bau GEEICHT, wo die Wahrheit danebenliegt: `--calib` sagt je Kamera, um wieviele Zeilen der
   Luminanzdetektor die bekannte Kante verfehlt. Gesucht wird ein KNICK, nicht ein Kontrast: der
   Himmel ist ein glatter Verlauf, seine zweite Differenz in y liegt bei 0,5 Anzeigecodes ueber 250
   Zeilen; eine Gelaendekante ist eine Stufe. Dunst DAEMPFT die Stufe, macht den Himmel aber nicht
   knickig.

3. WOLKEN. Die sechs Kamera-Szenen deklarieren `cloudCover: 0.0`, also ist die Kante des Baus
   Gelaende. Im Foto ist sie es nicht ueberall, und `--photo` misst das, statt es zu glauben: eine
   Wolke steht UEBER dem Grat, der Streifen zwischen Fotokante und Baukante ist dann heller und
   entsaettigter als der Fels darunter. Beide Zahlen stehen in der Ausgabe.

4. AUFLOESUNG. 320x180 kann die Frage nicht beantworten: bei 63,55 Grad ist ein Pixel dort 3,5 mrad,
   auf 45 km also 156 m; die Netzdichten, um die es geht, liegen bei 13 bis 153 m. Voll aufgeloest ist
   ein Pixel 0,87 mrad = 39 m auf 45 km.
"""
import argparse
import hashlib
import json
import pathlib
import subprocess
import sys

import numpy as np
from PIL import Image

SIM = pathlib.Path(__file__).resolve().parent.parent
CAMS = ["nebelhorn", "herzogstand", "innsbruck", "hochries", "zugspitze", "hochkoenig"]
W, H = 1280, 720
YW = np.array([0.2126, 0.7152, 0.0722], np.float64)
KNEE = 3.0   # Vielfache des Himmelsrauschens; geeicht in --calib


def rgb(path):
    im = Image.open(path).convert("RGB")
    if im.size != (W, H):
        im = im.resize((W, H), Image.LANCZOS)
    return np.asarray(im, np.float64)


def lum(a):
    return a[:, :, 0] * YW[0] + a[:, :, 1] * YW[1] + a[:, :, 2] * YW[2]


def skyline_depth(path):
    d = np.fromfile(path, np.float32).reshape(H, W)
    hit = d > 0.0
    return np.where(hit.any(0), hit.argmax(0), H - 1).astype(np.int32)


def skyline_lum(a, sky):
    """Oberste Zeile je Spalte, deren zweite Differenz in y das Himmelsrauschen reisst.

    Der Massstab wird JE SPALTE aus deren eigenem Himmel genommen: ein globales Band faengt an den
    Bildraendern die dort hoeher stehenden Grate mit ein, und dann ist die Schwelle eine Gelaendestufe
    statt eines Rauschens. `sky` = je Spalte die letzte Zeile, die sicher Himmel ist."""
    l = lum(a)
    k = np.abs(l[2:] - 2.0 * l[1:-1] + l[:-2])
    y = np.full(W, H - 1, np.int32)
    thr = np.zeros(W)
    for x in range(W):
        n = int(sky[x])
        if n < 12:
            return None, 0.0   # kein Himmelsband ueber der Kante: nichts zu eichen
        t = KNEE * (float(np.percentile(k[:n, x], 99)) + 0.5)
        thr[x] = t
        h = np.nonzero(k[:, x] > t)[0]
        if len(h):
            y[x] = h[0] + 1
    return y, float(np.median(thr))


def stats(d):
    a = np.abs(d)
    return float(np.median(a)), float(np.percentile(a, 95)), float(a.max()), float(np.median(d))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render")
    ap.add_argument("--out")
    ap.add_argument("--a")
    ap.add_argument("--b")
    ap.add_argument("--calib", action="store_true")
    ap.add_argument("--photo", action="store_true")
    ap.add_argument("--only")
    o = ap.parse_args()
    cams = {c["slug"]: c for c in json.loads((SIM.parent / "mods/webcams/cams.json").read_text())["cams"]}
    want = o.only.split(",") if o.only else CAMS

    if o.render:
        d = pathlib.Path(o.out)
        d.mkdir(parents=True, exist_ok=True)
        print("bin md5", hashlib.md5(pathlib.Path(o.render).read_bytes()).hexdigest())
        for s in want:
            if (d / f"{s}-fit.f32").exists():
                continue
            subprocess.run([str(pathlib.Path(o.render).resolve()),
                            "--scene", str(SIM / "web" / "cams" / f"{s}-fit-scene.json"),
                            "--out", str(d / f"{s}-fit.png"), "--depth", str(d / f"{s}-fit.f32"),
                            "--warm", "20000",
                            "--eye-asl", f"{float(cams[s]['altM']):.2f}", "--size", f"{W}x{H}"],
                           capture_output=True, text=True, cwd=str(SIM))
        return 0

    A = pathlib.Path(o.a)
    if o.calib:
        print(f"{'Kamera':13s} {'Kante Tiefe':>12s} {'|dZeile| Detektor':>18s} {'p95':>6s} "
              f"{'Treffer <=1px*':>14s} {'Schwelle':>9s}")
        for s in want:
            yd = skyline_depth(A / f"{s}-fit.f32")
            yl, thr = skyline_lum(rgb(A / f"{s}-fit.png"), yd // 2)
            if yl is None:
                print(f"{s:13s} {yd.mean():12.1f} {'kein Himmelsband':>18s}")
                continue
            d = yl.astype(np.float64) - yd
            med, p95, mx, bias = stats(d)
            print(f"{s:13s} {yd.mean():12.1f} {med:18.2f} {p95:6.1f} "
                  f"{float((np.abs(d - bias) <= 1).mean()) * 100:12.1f}% {thr:9.2f}  Versatz {bias:+.0f}")
        return 0

    if o.photo:
        print(f"{'Kamera':13s} {'Bau Zeile':>10s} {'Foto Zeile':>11s} {'Foto-Bau':>9s} "
              f"{'Foto hoeher':>12s} {'Streifen dL':>12s} {'dSaettigung':>12s}")
        for s in want:
            yd = skyline_depth(A / f"{s}-fit.f32")
            p = rgb(SIM / "web" / "cams" / f"{s}-fit.jpg")
            yp, _ = skyline_lum(p, yd // 2)
            if yp is None:
                print(f"{s:13s} {yd.mean():10.1f}   kein Himmelsband ueber der Kante")
                continue
            d = yp.astype(np.float64) - yd
            med, p95, mx, bias = stats(d)
            dl, ds, n = 0.0, 0.0, 0
            for x in range(W):
                if yp[x] < yd[x] - 2:
                    band, rock = p[yp[x]:yd[x], x], p[yd[x]:min(H, yd[x] + 20), x]
                    if len(band) and len(rock):
                        dl += band.mean() - rock.mean()
                        ds += (band.max(1) - band.min(1)).mean() - (rock.max(1) - rock.min(1)).mean()
                        n += 1
            print(f"{s:13s} {yd.mean():10.1f} {yp.mean():11.1f} {bias:+9.0f} "
                  f"{float((d < -2).mean()) * 100:11.1f}% {(dl / n if n else 0):12.1f} "
                  f"{(ds / n if n else 0):12.1f}")
        return 0

    B = pathlib.Path(o.b)
    print(f"{'Kamera':13s} {'A Zeile':>8s} {'B Zeile':>8s} {'|dZeile| med':>13s} {'p95':>6s} "
          f"{'max':>5s} {'bewegt':>7s}")
    for s in want:
        ya, yb = skyline_depth(A / f"{s}-fit.f32"), skyline_depth(B / f"{s}-fit.f32")
        med, p95, mx, _ = stats(yb.astype(np.float64) - ya)
        print(f"{s:13s} {ya.mean():8.1f} {yb.mean():8.1f} {med:13.2f} {p95:6.1f} {mx:5.0f} "
              f"{float((ya != yb).mean()) * 100:6.1f}%")
    return 0


if __name__ == "__main__":
    sys.exit(main())
