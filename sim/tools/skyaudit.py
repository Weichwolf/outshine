#!/usr/bin/env python3
"""Die vier Abnahmezahlen des Himmel-/Belichtungssystems, Render gegen Foto.

  tools/skyaudit.py                       # web/cams/<slug>-fit.png gegen -fit.jpg
  tools/skyaudit.py --render build/gpu_walk --out /tmp/audit

1 B-R vom Zenit (Zeile 2) zur Zeile 45, Spalten 120..200 — muss fallen.
2 Anteil Pixel mit L>200.
3 Mittlerer lokaler Gradient (|dx|+|dy| auf L) in der unteren Bildhaelfte.
4 Mittleres RGB der dunkelsten 2 % — soll B>G>R und L>=20 sein.
"""
import argparse, hashlib, json, os, pathlib, subprocess, sys

SIM = pathlib.Path(__file__).resolve().parent.parent
CAMS = ["nebelhorn", "herzogstand", "innsbruck", "hochries", "zugspitze", "hochkoenig"]
W, H = 320, 180


def load(p):
    from PIL import Image
    im = Image.open(p).convert("RGB").resize((W, H), Image.LANCZOS)
    return [list(im.getdata())[y * W:(y + 1) * W] for y in range(H)]


def lum(px):
    return 0.2126 * px[0] + 0.7152 * px[1] + 0.0722 * px[2]


def band_br(rows, y):
    s = [rows[y][x][2] - rows[y][x][0] for x in range(120, 201)]
    return sum(s) / len(s)


def metrics(rows):
    br2, br45 = band_br(rows, 2), band_br(rows, 45)
    flat = [p for r in rows for p in r]
    bright = sum(1 for p in flat if lum(p) > 200) / len(flat)
    g = []
    for y in range(H // 2, H - 1):
        for x in range(W - 1):
            l0 = lum(rows[y][x])
            g.append(abs(lum(rows[y][x + 1]) - l0) + abs(lum(rows[y + 1][x]) - l0))
    grad = sum(g) / len(g)
    ordered = sorted(flat, key=lum)[:max(1, int(0.02 * len(flat)))]
    n = len(ordered)
    dark = tuple(sum(p[i] for p in ordered) / n for i in range(3))
    return {"br_zenith": br2, "br_45": br45, "monotone": br45 < br2,
            "bright_frac": bright, "grad": grad, "dark": dark, "dark_L": lum(dark),
            "dark_blue": dark[2] > dark[1] > dark[0]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render")
    ap.add_argument("--out")
    ap.add_argument("--only")
    a = ap.parse_args()
    want = a.only.split(",") if a.only else CAMS
    outdir = pathlib.Path(a.out) if a.out else SIM / "web" / "cams"
    outdir.mkdir(parents=True, exist_ok=True)
    if a.render:
        print("bin md5", hashlib.md5(pathlib.Path(a.render).read_bytes()).hexdigest())
    res = {}
    for s in want:
        png = outdir / f"{s}-fit.png"
        if a.render:
            # THE SCENE IS DECLARED (mods/webcams). Only the artifact root is this tool's.
            subprocess.run([str(pathlib.Path(a.render).resolve()), "webcams", f"{s}-fit"],
                           capture_output=True, text=True, cwd=str(SIM),
                           env=dict(os.environ, OUTSHINE_OUT=str(outdir)))
        r = metrics(load(png))
        p = metrics(load(SIM / "web" / "cams" / f"{s}-fit.jpg"))
        res[s] = {"render": r, "photo": p}
        print(f"{s:12s} B-R zen/45 R {r['br_zenith']:+7.1f} ->{r['br_45']:+7.1f} "
              f"{'FALL' if r['monotone'] else 'STEIG'}   F {p['br_zenith']:+7.1f} ->{p['br_45']:+7.1f} "
              f"{'FALL' if p['monotone'] else 'STEIG'}")
        print(f"{'':12s} L>200    R {r['bright_frac']:.4f}   F {p['bright_frac']:.4f}"
              f"   | grad R {r['grad']:6.2f} F {p['grad']:6.2f} ({r['grad']/max(p['grad'],1e-9):.2f}x)")
        print(f"{'':12s} dark2%   R ({r['dark'][0]:5.1f},{r['dark'][1]:5.1f},{r['dark'][2]:5.1f}) "
              f"L{r['dark_L']:5.1f} {'blau' if r['dark_blue'] else 'NICHT blau'}   "
              f"F ({p['dark'][0]:5.1f},{p['dark'][1]:5.1f},{p['dark'][2]:5.1f}) L{p['dark_L']:5.1f}")
    ok1 = sum(1 for s in res if res[s]["render"]["monotone"])
    ok2 = sum(1 for s in res if res[s]["render"]["bright_frac"] > 0)
    ok3 = sum(1 for s in res if res[s]["render"]["grad"] >= 0.5 * res[s]["photo"]["grad"])
    ok4 = sum(1 for s in res if res[s]["render"]["dark_blue"] and res[s]["render"]["dark_L"] >= 20)
    print(f"\nAbnahme: 1 monoton {ok1}/6 (>=5)  2 L>200 {ok2}/6 (>=4)  "
          f"3 Gradient>=halb {ok3}/6  4 Tiefen blau&L>=20 {ok4}/6")
    return 0


if __name__ == "__main__":
    sys.exit(main())
