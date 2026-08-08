#!/usr/bin/env python3
"""Der gleiche z14-Ausschnitt dreifach: OSM-Bake, unsere Aufnahme, Luftbild.

  tools/tilecompare.py --bin build/gpu_walk --out /tmp/cmp

Fuer jeden Prueforten wird eine Szene an die Kachelmitte geschrieben, orthografisch von oben
gerendert und neben die beiden Referenzen des Kachelservers gelegt. Stufe 1 fragt nach FORM, also
rendert der Vergleichslauf die Klassenvisualisierung; --shaded nimmt stattdessen das Material.

Exit 0, wenn jeder Ort seine drei Bilder hat.
"""
import argparse
import sys as _sys, pathlib as _pl
_sys.path.insert(0, str(_pl.Path(__file__).resolve().parent))
import runscene, hashlib, json, math, os, pathlib, subprocess, sys, urllib.request

Z = 14
ORTS = [
    ("weserbergland", 52.10602, 9.43453, "offenes Land, die Referenzszene"),
    ("hameln",        52.10390, 9.35630, "Kleinstadt an der Weser"),
    ("hannover",      52.37590, 9.73200, "Grossstadt, dichte Bebauung"),
    ("luebeck",       53.86550, 10.68660, "Hansestadt, Wasser und Altstadt"),
    ("allgaeu",       47.58000, 10.29000, "Voralpen, starkes Relief"),
    ("lueneburg",     53.14000, 10.26000, "Heide und Forst, andere Landbedeckung"),
]


def tile_of(lat, lon, z=Z):
    n = 2 ** z
    x = int((lon + 180.0) / 360.0 * n)
    y = int((1.0 - math.log(math.tan(math.radians(lat)) + 1.0 / math.cos(math.radians(lat))) / math.pi) / 2.0 * n)
    return x, y


def tile_bounds(x, y, z=Z):
    """Die Kachel in Grad, und ihre Ost-West-Ausdehnung am Boden in Metern."""
    n = 2 ** z
    lon0, lon1 = x / n * 360.0 - 180.0, (x + 1) / n * 360.0 - 180.0
    lat0 = math.degrees(math.atan(math.sinh(math.pi * (1 - 2 * y / n))))
    lat1 = math.degrees(math.atan(math.sinh(math.pi * (1 - 2 * (y + 1) / n))))
    latm, lonm = 0.5 * (lat0 + lat1), 0.5 * (lon0 + lon1)
    widthM = 40075016.686 / n * math.cos(math.radians(latm))
    return latm, lonm, widthM


def fetch(url, path, timeout=120):
    for _ in range(80):                      # 202 = der Server holt gerade; nur 200 traegt Bytes
        try:
            r = urllib.request.urlopen(url, timeout=timeout)
            d = r.read()
            if r.status == 200 and len(d) > 512:
                pathlib.Path(path).write_bytes(d)
                return len(d)
        except Exception:
            pass
        import time; time.sleep(0.25)
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default="build/gpu_walk")
    ap.add_argument("--out", default="/tmp/tilecompare")
    ap.add_argument("--base", default="http://localhost:8081")
    ap.add_argument("--size", type=int, default=512)
    ap.add_argument("--warm", type=int, default=1500)
    ap.add_argument("--eye", type=float, default=2500.0)
    ap.add_argument("--shaded", action="store_true", help="Material statt Klassenvisualisierung")
    ap.add_argument("--only", default="", help="nur diese Orte, kommagetrennt")
    args = ap.parse_args()

    binp = os.path.abspath(args.bin)
    md5 = hashlib.md5(open(binp, "rb").read()).hexdigest()
    out = pathlib.Path(args.out); out.mkdir(parents=True, exist_ok=True)
    print(f"binary {binp}  md5 {md5}")
    print(f"{'Ort':16s} {'Kachel':16s} {'Breite':>9s} {'Bake':>8s} {'Luftbild':>9s}  unsere")

    want = [o for o in ORTS if not args.only or o[0] in args.only.split(",")]
    missing = []
    for name, lat, lon, _why in want:
        x, y = tile_of(lat, lon)
        latm, lonm, widthM = tile_bounds(x, y)
        bake = out / f"{name}-1-bake.png"
        photo = out / f"{name}-3-luftbild.png"
        ours = out / f"{name}-2-outshine.png"
        nb = fetch(f"{args.base}/bake/osm/{Z}/{x}/{y}?tex={args.size}", bake)
        np_ = fetch(f"{args.base}/t/imagery/{Z}/{x}/{y}", photo)

        # ONE DECLARED SCENE PER KACHEL, straight down and orthographic: the tile's own width is
        # the picture's width, so the render and the bake are the same square of ground.
        scene = runscene.still(name, latm, lonm, ours.name,
                               eyeM=float(args.eye), pitchDeg=-90, orthoM=round(widthM, 2),
                               capture={"width": args.size, "height": args.size,
                                        "warmCeiling": args.warm})
        env = {} if args.shaded else {"FB_GROUND_CLASS_VIZ": "1"}
        r = runscene.run(binp, scene, out_root=str(out), env=env)
        ok = ours.exists() and ours.stat().st_size > 512
        print(f"{name:16s} {Z}/{x}/{y:<9} {widthM:8.1f}m {nb:8d} {np_:9d}  {'ok' if ok else 'FEHLT'}")
        if not ok:
            missing.append(name)
            err = [l for l in (r.stderr + r.stdout).splitlines() if "ERROR" in l][:2]
            for l in err: print("     " + l[:160])

    print(f"\n{out}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
