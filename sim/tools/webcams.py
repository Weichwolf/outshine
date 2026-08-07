#!/usr/bin/env python3
"""Das staendige Feld: je Kamera das Livebild und unser Render nebeneinander, 320x180.

  tools/webcams.py --bin build/gpu_walk            # alle 24
  tools/webcams.py --bin build/gpu_walk --only zugspitze,nebelhorn

Schreibt web/cams/ mit den Bildpaaren und index.html. Die Seite ist der Stand — sie wird
angesehen, nicht berichtet.
"""
import argparse, datetime, hashlib, json, os, pathlib, subprocess, sys, urllib.request

W, H = 320, 180
SIM = pathlib.Path(__file__).resolve().parent.parent
OUT = SIM / "web" / "cams"
BASE = "https://www.foto-webcam.eu/webcam"
UA = {"User-Agent": "Mozilla/5.0 (outshine reference harness)"}


def live(slug, dst):
    """Das aktuelle Bild und, wenn der Server ihn nennt, sein Zeitstempel."""
    req = urllib.request.Request(f"{BASE}/{slug}/current/1920.jpg", headers=UA)
    try:
        r = urllib.request.urlopen(req, timeout=30)
        d = r.read()
        if len(d) < 10000:
            return None, None
        dst.write_bytes(d)
        lm = r.headers.get("Last-Modified")
        if lm:
            t = datetime.datetime.strptime(lm, "%a, %d %b %Y %H:%M:%S %Z")
            return len(d), t.replace(tzinfo=datetime.timezone.utc)
        return len(d), datetime.datetime.now(datetime.timezone.utc)
    except Exception:
        return None, None


def fogged(path):
    """Wolke oder Nebel: wenig Buntheit UND wenig Kontrast ueber das ganze Bild. Gegen ein Nebelbild
    zu rendern misst nichts — der Unterschied waere dann das Wetter und nicht die Engine."""
    try:
        import subprocess, json as _j
        r = subprocess.run(["python3", "-c", _PROBE, str(path)], capture_output=True, text=True)
        d = _j.loads(r.stdout)
    except Exception:
        return False, {}
    return (d["sat"] < 0.10 and d["p95p5"] < 70), d


_PROBE = r"""
import sys, struct, zlib
# Ein JPEG ohne Fremdbibliothek zu dekodieren waere unverhaeltnismaessig; stattdessen die DC-Statistik
# ueber die Datei selbst: Buntheit und Kontacht schaetzen wir aus dem heruntergerechneten PNG, das der
# Renderer ohnehin schreibt. Hier reicht die Dateigroesse NICHT, also nutzen wir sips (macOS).
import subprocess, json, os, tempfile
src = sys.argv[1]
tmp = tempfile.mktemp(suffix=".png")
subprocess.run(["sips", "-s", "format", "png", "-Z", "64", src, "--out", tmp],
               capture_output=True)
d = open(tmp, "rb").read(); os.unlink(tmp)
i = 8; w = h = 0; idat = b""
while i < len(d):
    ln = struct.unpack(">I", d[i:i+4])[0]; t = d[i+4:i+8]
    if t == b"IHDR": w, h, bd, ct = struct.unpack(">IIBB", d[i+8:i+18])
    elif t == b"IDAT": idat += d[i+8:i+8+ln]
    i += 12 + ln
raw = zlib.decompress(idat); bpp = 4 if ct == 6 else 3; stride = w*bpp
out = bytearray(); prev = bytearray(stride); o = 0
for y in range(h):
    f = raw[o]; o += 1; line = bytearray(raw[o:o+stride]); o += stride
    for x in range(stride):
        a = line[x-bpp] if x >= bpp else 0; b = prev[x]; c = prev[x-bpp] if x >= bpp else 0
        if f == 1: line[x] = (line[x]+a) & 255
        elif f == 2: line[x] = (line[x]+b) & 255
        elif f == 3: line[x] = (line[x]+(a+b)//2) & 255
        elif f == 4:
            pa = abs(b-c); pb = abs(a-c); pc = abs(a+b-2*c)
            pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
            line[x] = (line[x]+pr) & 255
    out += line; prev = line
lum = []; sat = 0.0; n = 0
for i2 in range(0, len(out), bpp):
    r, g, b2 = out[i2], out[i2+1], out[i2+2]
    mx, mn = max(r, g, b2), min(r, g, b2)
    sat += (mx-mn)/max(mx, 1); n += 1
    lum.append(0.2126*r+0.7152*g+0.0722*b2)
lum.sort()
print(json.dumps({"sat": sat/max(n,1), "p95p5": lum[19*len(lum)//20]-lum[len(lum)//20]}))
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default="build/gpu_walk")
    ap.add_argument("--only", default="")
    ap.add_argument("--warm", type=int, default=12000)
    args = ap.parse_args()

    cams = json.loads((SIM / "assets/world/webcams.json").read_text())["cams"]
    if args.only:
        want = set(args.only.split(","))
        cams = [c for c in cams if c["slug"] in want]

    binp = os.path.abspath(args.bin)
    md5 = hashlib.md5(open(binp, "rb").read()).hexdigest()
    OUT.mkdir(parents=True, exist_ok=True)
    print(f"binary {binp}  md5 {md5}")
    print(f"{'Kamera':16s} {'live':>8s} {'Zeit (UTC)':>17s} {'Sonne':>7s}  Render")

    rows = []
    for c in cams:
        slug = c["slug"]
        photo = OUT / f"{slug}-live.jpg"
        shot = OUT / f"{slug}-outshine.png"
        n, when = live(slug, photo)
        fog, st = (fogged(photo) if n else (False, {}))
        if n is None:
            print(f"{slug:16s} {'FEHLT':>8s}")
            rows.append((c, None, None, False))
            continue

        scene = OUT / f"{slug}-scene.json"
        scene.write_text(json.dumps({
            "lat": c["lat"], "lon": c["lon"], "eyeM": 3.0,
            "yawDeg": c["yawDeg"], "pitchDeg": c["pitchDeg"], "fovDeg": c["fovDeg"],
            "utc": when.strftime("%Y-%m-%dT%H:%M:%SZ"),
            "windDeg": 250, "windMs": 4.0, "cloudCover": 0.0}, indent=2))

        r = subprocess.run(
            [binp, "--scene", str(scene), "--out", str(shot), "--warm", str(args.warm),
             "--eye", "3.0", "--size", f"{W}x{H}"],
            capture_output=True, text=True, cwd=str(SIM))
        sun = ""
        for l in (r.stdout + r.stderr).splitlines():
            if "sunElDeg=" in l and "irradiance" in l:
                sun = l.split("sunElDeg=")[1].split()[0][:5]
        ok = shot.exists() and shot.stat().st_size > 1000
        mark = "in Wolken" if fog else ""
        print(f"{slug:16s} {n:8d} {when.strftime('%Y-%m-%d %H:%M'):>17s} {sun:>7s}  {'ok' if ok else 'FEHLT'} {mark}")
        rows.append((c, when, ok, fog))

    html = ["<!doctype html><meta charset=utf-8><title>Outshine gegen foto-webcam.eu</title>",
            "<style>body{background:#111;color:#ccc;font:13px/1.4 system-ui;margin:16px}",
            "h1{font-size:15px;font-weight:600;margin:0 0 4px}",
            ".n{color:#888;margin:0 0 16px}",
            ".g{display:grid;grid-template-columns:repeat(auto-fill,minmax(660px,1fr));gap:14px}",
            ".c{background:#191919;padding:8px;border-radius:4px}",
            ".p{display:flex;gap:6px}.p img{width:320px;height:180px;object-fit:cover;background:#000}",
            ".t{display:flex;justify-content:space-between;margin-bottom:4px}",
            ".w{color:#c86}</style>",
            "<h1>Outshine gegen foto-webcam.eu &mdash; links Livebild, rechts unser Render</h1>",
            f"<p class=n>Binary <code>{md5}</code> &middot; 320&times;180 &middot; "
            f"erzeugt {datetime.datetime.now(datetime.timezone.utc):%Y-%m-%d %H:%M} UTC</p>",
            "<div class=g>"]
    for c, when, ok, fog in rows:
        fit = "" if c.get("fitted") else "<span class=w>Pose ungefittet</span>"
        if fog: fit += " <span class=w>in Wolken</span>"
        ts = when.strftime("%H:%M UTC") if when else "&mdash;"
        html.append(f"<div class=c><div class=t><b>{c['name']}</b><span>{ts} {fit}</span></div>"
                    f"<div class=p><img src='{c['slug']}-live.jpg'>"
                    f"<img src='{c['slug']}-outshine.png'></div></div>")
    html.append("</div>")
    (OUT / "index.html").write_text("\n".join(html))
    print(f"\n{OUT/'index.html'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
