#!/usr/bin/env python3
"""Das staendige Feld: je Kamera das Livebild und unser Render nebeneinander, 320x180.

  tools/webcams.py --bin build/gpu_walk            # alle 24
  tools/webcams.py --bin build/gpu_walk --only zugspitze,nebelhorn

Schreibt web/cams/ mit den Bildpaaren und index.html. Die Seite ist der Stand — sie wird
angesehen, nicht berichtet.
"""
import argparse, datetime, hashlib, json, os, pathlib, re, subprocess, sys, urllib.request
import zoneinfo

W, H = 320, 180
SIM = pathlib.Path(__file__).resolve().parent.parent
OUT = SIM / "web" / "cams"
BASE = "https://www.foto-webcam.eu/webcam"
UA = {"User-Agent": "Mozilla/5.0 (outshine reference harness)"}
MAX_AGE_S = 3600
CAM_TZ = zoneinfo.ZoneInfo("Europe/Berlin")
ARCHIVE = re.compile(r'"(20\d\d)\\?/(\d\d)\\?/(\d\d)\\?/(\d\d)(\d\d)_hd\.jpg"')


def get(url, timeout=30):
    return urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout)


def header_time(slug):
    """Nur zur Gegenprobe: der Last-Modified von current/1920.jpg."""
    try:
        lm = get(f"{BASE}/{slug}/current/1920.jpg", 20).headers.get("Last-Modified")
        return datetime.datetime.strptime(lm, "%a, %d %b %Y %H:%M:%S %Z").replace(
            tzinfo=datetime.timezone.utc)
    except Exception:
        return None


def live(slug, dst):
    """Das juengste Archivbild: sein Zeitstempel steht in seinem eigenen Pfad, in Ortszeit der Kamera.

    Nicht current/1920.jpg — dessen Last-Modified ist der einzige Zeuge seiner selbst, und bei einer
    abgeschalteten Kamera zeigt er jahrealt weiter auf das letzte Bild. Das Bild selbst traegt keine
    eingebrannte Zeit (gemessen an zugspitze 2026-08-07: weder in den oberen 90 noch in den unteren
    60 Zeilen steht Text), also ist der Pfad die einzige selbstbeschreibende Quelle."""
    try:
        page = get(f"{BASE}/{slug}/", 30).read().decode("utf-8", "replace")
    except Exception as e:
        return None, None, f"Seite nicht erreichbar: {e}"
    m = ARCHIVE.search(page)
    if not m:
        return None, None, "kein Archivbild auf der Seite"
    y, mo, d, hh, mm = (int(v) for v in m.groups())
    when = datetime.datetime(y, mo, d, hh, mm, tzinfo=CAM_TZ).astimezone(datetime.timezone.utc)
    age = (datetime.datetime.now(datetime.timezone.utc) - when).total_seconds()
    if abs(age) > MAX_AGE_S:
        return None, when, f"kein Livebild: {age / 3600.0:.1f} h alt ({when:%Y-%m-%d %H:%M} UTC)"
    try:
        data = get(f"{BASE}/{slug}/{y:04d}/{mo:02d}/{d:02d}/{hh:02d}{mm:02d}_hd.jpg", 30).read()
    except Exception as e:
        return None, when, f"Archivbild nicht abrufbar: {e}"
    if len(data) < 10000:
        return None, when, f"Archivbild zu klein ({len(data)} B)"
    dst.write_bytes(data)
    lm = header_time(slug)
    if lm and abs((lm - when).total_seconds()) > MAX_AGE_S:
        return len(data), when, f"Header weicht ab: {lm:%Y-%m-%d %H:%M} UTC gegen Pfad"
    return len(data), when, ""


def fogged(path):
    """Wolke oder Nebel: wenig Buntheit UND wenig Kontrast. Gegen ein Nebelbild zu rendern misst
    nichts -- der Unterschied waere dann das Wetter und nicht die Engine."""
    import numpy as np
    from PIL import Image
    a = np.asarray(Image.open(path).convert("RGB").resize((256, 144)), np.float32)
    mx, mn = a.max(2), a.min(2)
    sat = float(((mx - mn) / np.maximum(mx, 1.0)).mean())
    lum = 0.2126 * a[:, :, 0] + 0.7152 * a[:, :, 1] + 0.0722 * a[:, :, 2]
    con = float(np.percentile(lum, 95) - np.percentile(lum, 5))
    return sat < 0.10 and con < 70.0, sat, con


def fit_shot(c, dst):
    """Das Bild, auf das die Pose eingepasst wurde, mit seiner eigenen Uhr.

    Das Livebild kommt zu jeder Tageszeit, und um Mitternacht steht auf beiden Seiten Nacht. Die
    Guete der Pose ist dann nicht zu sehen; das Fitbild zeigt sie."""
    when = c.get("fitImage")
    if not when:
        return None
    y, mo, d, hm = when.split("/")
    t = datetime.datetime(int(y), int(mo), int(d), int(hm[:2]), int(hm[2:]),
                          tzinfo=CAM_TZ).astimezone(datetime.timezone.utc)
    if not dst.exists() or dst.stat().st_size < 10000:
        try:
            dst.write_bytes(get(f"{BASE}/{c['slug']}/{when}_hd.jpg", 60).read())
        except Exception:
            return None
    return t


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default="build/gpu_walk")
    ap.add_argument("--only", default="")
    ap.add_argument("--warm", type=int, default=12000)
    args = ap.parse_args()

    cams = json.loads((SIM.parent / "mods/webcams/cams.json").read_text())["cams"]
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
        n, when, why = live(slug, photo)
        fog, sat, con = (fogged(photo) if n else (False, 0.0, 0.0))
        if n is None:
            photo.unlink(missing_ok=True)
            shot.unlink(missing_ok=True)
            print(f"{slug:16s} {'VERWORFEN':>9s}  {why}")
            rows.append((c, when, None, False, 0.0))
            continue
        if why:
            print(f"{slug:16s} {'WARNUNG':>9s}  {why}")

        # Die Augenhoehe ist gemessen, nicht gesetzt: der Betreiber nennt die Objektivhoehe ueber
        # NN, /elev nennt den Boden darunter. Zwei Meter Untergrenze, weil das DEM einen scharfen
        # Gipfel um bis zu 10 m abschneidet und die Kamera sonst im Berg staende.
        eye = max(float(c["altM"]) - float(c["groundM"]), 2.0)
        scene = OUT / f"{slug}-scene.json"
        scene.write_text(json.dumps({
            "lat": c["lat"], "lon": c["lon"], "eyeM": eye,
            "yawDeg": c["yawDeg"], "pitchDeg": c["pitchDeg"], "fovDeg": c["fovDeg"],
            "utc": when.strftime("%Y-%m-%dT%H:%M:%SZ"),
            "windDeg": 250, "windMs": 4.0, "cloudCover": 0.0}, indent=2))

        r = subprocess.run(
            [binp, "--scene", str(scene), "--out", str(shot), "--warm", str(args.warm),
             "--eye", f"{eye:.2f}", "--size", f"{W}x{H}"],
            capture_output=True, text=True, cwd=str(SIM))
        sun = ""
        for l in (r.stdout + r.stderr).splitlines():
            if "sunElDeg=" in l and "irradiance" in l:
                sun = l.split("sunElDeg=")[1].split()[0][:5]
        fit_photo = OUT / f"{slug}-fit.jpg"
        fit_shotpng = OUT / f"{slug}-fit.png"
        ft = fit_shot(c, fit_photo)
        if ft:
            fscene = OUT / f"{slug}-fit-scene.json"
            fscene.write_text(json.dumps({
                "lat": c["lat"], "lon": c["lon"], "eyeM": eye,
                "yawDeg": c["yawDeg"], "pitchDeg": c["pitchDeg"], "fovDeg": c["fovDeg"],
                "utc": ft.strftime("%Y-%m-%dT%H:%M:%SZ"),
                "windDeg": 250, "windMs": 4.0, "cloudCover": 0.0}, indent=2))
            subprocess.run(
                [binp, "--scene", str(fscene), "--out", str(fit_shotpng), "--warm", str(args.warm),
                 "--eye", f"{eye:.2f}", "--size", f"{W}x{H}"],
                capture_output=True, text=True, cwd=str(SIM))
        ok = shot.exists() and shot.stat().st_size > 1000
        mark = "in Wolken" if fog else ""
        print(f"{slug:16s} {n:8d} {when.strftime('%Y-%m-%d %H:%M'):>17s} {sun:>7s}  "
              f"Auge {eye:5.1f} m  {'ok' if ok else 'FEHLT'} {mark}")
        rows.append((c, when, ok, fog, eye))

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
    for c, when, ok, fog, eye in rows:
        if not ok:
            continue
        fit = (f"Klaffen {c['residPx']} px" if c.get("fitted")
               else f"<span class=w>Pose ungefittet ({c.get('residPx', '?')} px)</span>")
        if fog: fit += " <span class=w>in Wolken</span>"
        ts = when.strftime("%H:%M UTC") if when else "&mdash;"
        pair2 = ""
        if (OUT / f"{c['slug']}-fit.png").exists():
            pair2 = (f"<div class=p><img src='{c['slug']}-fit.jpg'>"
                     f"<img src='{c['slug']}-fit.png'></div>")
        html.append(f"<div class=c><div class=t><b>{c['name']}</b>"
                    f"<span>{ts} &middot; {fit}</span></div>"
                    f"<div class=p><img src='{c['slug']}-live.jpg'>"
                    f"<img src='{c['slug']}-outshine.png'></div>{pair2}"
                    f"<div class=n>{c['altM']} m ueber NN, {eye:.0f} m ueber Grund &middot; "
                    f"Azimut {c['yawDeg']:.1f}&deg;, Neigung {c['pitchDeg']:.1f}&deg;, "
                    f"Bildwinkel {c['fovDeg']:.1f}&deg;"
                    + (f" &middot; Einpassbild {c['fitImage']} Ortszeit" if c.get("fitImage") else "")
                    + "</div></div>")
    html.append("</div>")
    dead = [c["name"] for c, _w, ok, _f, _e in rows if not ok]
    if dead:
        html.append(f"<p class=n>Ohne Livebild und darum nicht gerendert: {', '.join(dead)}</p>")
    (OUT / "index.html").write_text("\n".join(html))
    print(f"\n{OUT/'index.html'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
