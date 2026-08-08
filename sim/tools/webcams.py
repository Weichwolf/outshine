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
# Unter dieser Sonnenhoehe traegt ein Paar nichts bei. Nicht der Horizont entscheidet, sondern das
# Gelaende: unter 5 Grad steht ein Berghang in seinem eigenen Schatten und der Vergleich misst die
# Daemmerung statt der Engine. Bei 47 Grad Nord wird 5 Grad an jedem Tag des Jahres erreicht.
DAY_SUN_EL_DEG = 5.0
# So weit zurueck wird nach dem juengsten Tagbild gesucht, in Stunden. 30 deckt eine Winternacht
# und einen bedeckten Vormittag ab, ohne das Archiv zu durchpfluegen.
DAY_LOOKBACK_H = 30
CAM_TZ = zoneinfo.ZoneInfo("Europe/Berlin")
ARCHIVE = re.compile(r'"(20\d\d)\\?/(\d\d)\\?/(\d\d)\\?/(\d\d)(\d\d)_hd\.jpg"')


def get(url, timeout=30, tries=3):
    """Systemgrenze, also defensiv: foto-webcam.eu antwortet zeitweise 502, und ein einzelner Versuch
    haette die ganze Seite geleert. Kurzer Backoff, dann gilt der Fehler."""
    import time
    for k in range(tries):
        try:
            return urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout)
        except Exception:
            if k + 1 == tries:
                raise
            time.sleep(1.5 * (k + 1))


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


def sun_el_deg(lat, lon, when):
    """Sonnenhoehe in Grad. NOAA-Naeherung (Michalsky/Spencer), Fehler unter 0,01 Grad im Band, das
    hier entscheidet — es geht um Tag oder Nacht, nicht um eine Ephemeride. Die Ephemeride des
    Renderers ist die des Bildes; diese hier waehlt nur das Bild aus."""
    import math
    jd = when.timestamp() / 86400.0 + 2440587.5
    n = jd - 2451545.0
    L = math.radians((280.460 + 0.9856474 * n) % 360.0)
    g = math.radians((357.528 + 0.9856003 * n) % 360.0)
    lam = L + math.radians(1.915) * math.sin(g) + math.radians(0.020) * math.sin(2 * g)
    eps = math.radians(23.439 - 4.0e-7 * n)
    dec = math.asin(math.sin(eps) * math.sin(lam))
    ra = math.atan2(math.cos(eps) * math.sin(lam), math.cos(lam))
    gmst = (18.697374558 + 24.06570982441908 * n) % 24.0
    ha = math.radians(gmst * 15.0 + lon) - ra
    la = math.radians(lat)
    return math.degrees(math.asin(math.sin(la) * math.sin(dec) +
                                  math.cos(la) * math.cos(dec) * math.cos(ha)))


def day_image(slug, lat, lon, newest, dst):
    """Das juengste TAGBILD, wenn das aktuelle Nacht ist. Ein Nachtpaar ist ein gueltiger Zustand und
    ein nutzloser Vergleich: beide Seiten sind schwarz. Statt es zu zeigen, tritt das juengste Bild
    mit Sonne ueber DAY_SUN_EL_DEG an seine Stelle -- ausdruecklich beschriftet, mit seiner eigenen
    Zeit. Der Archivraster ist zehn Minuten."""
    t = newest
    for _ in range(DAY_LOOKBACK_H * 6):
        t -= datetime.timedelta(minutes=10)
        if sun_el_deg(lat, lon, t) < DAY_SUN_EL_DEG:
            continue
        loc = t.astimezone(CAM_TZ)
        url = (f"{BASE}/{slug}/{loc:%Y/%m/%d}/{loc:%H%M}_hd.jpg")
        try:
            data = get(url, 30).read()
        except Exception:
            continue
        if len(data) < 10000:
            continue
        dst.write_bytes(data)
        return t
    return None


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

    statef = OUT / "state.json"
    state = json.loads(statef.read_text()) if statef.exists() else {}

    rows = []
    for c in cams:
        slug = c["slug"]
        photo = OUT / f"{slug}-live.jpg"
        shot = OUT / f"{slug}-outshine.png"
        n, when, why = live(slug, photo)
        fog, sat, con = (fogged(photo) if n else (False, 0.0, 0.0))
        if n is None:
            # DIE SEITE IST DER STAND, ALSO DARF SIE NICHT LEER WERDEN. Ein 502 der Gegenstelle ist
            # kein Grund, ein gutes Paar zu loeschen -- es bleibt stehen und traegt seine eigene Zeit
            # samt dem Grund, warum es nicht frisch ist.
            old = state.get(slug)
            if old and photo.exists() and shot.exists():
                print(f"{slug:16s} {'ALT':>9s}  {why} — letzter Stand {old['shown']} bleibt stehen")
                keep = datetime.datetime.fromisoformat(old["shown"])
                rows.append((c, keep, keep, True, old.get("fog", False), old.get("eye", 0.0),
                             old.get("lift", 0.0), old.get("cleared", 0),
                             f"nicht aktualisiert: {why}", old.get("roof", False)))
                continue
            photo.unlink(missing_ok=True)
            shot.unlink(missing_ok=True)
            print(f"{slug:16s} {'VERWORFEN':>9s}  {why}")
            rows.append((c, when, when, None, False, 0.0, 0.0, 0, "", False))
            continue
        if why:
            print(f"{slug:16s} {'WARNUNG':>9s}  {why}")

        # NACHT IST EIN GUELTIGER ZUSTAND UND EIN NUTZLOSER VERGLEICH. Steht die Sonne unter
        # DAY_SUN_EL_DEG, tritt das juengste Tagbild an die Stelle des Livebildes -- beschriftet,
        # mit seiner eigenen Zeit. Der Zeitstempel des Livebildes bleibt daneben stehen.
        shown, sub = when, ""
        el_live = sun_el_deg(c["lat"], c["lon"], when)
        if el_live < DAY_SUN_EL_DEG:
            t = day_image(slug, c["lat"], c["lon"], when, photo)
            if t:
                shown, sub = t, f"Nacht ({el_live:+.0f}\u00b0), juengstes Tagbild"
                fog, sat, con = fogged(photo)
            else:
                sub = f"Nacht ({el_live:+.0f}\u00b0), kein Tagbild im Archiv"

        # DER STANDPUNKT WIRD GEPRUEFT, NICHT GESETZT. `altM` ist die Objektivhoehe des Betreibers
        # und damit das Datum; die Hoehe ueber Grund faellt im Renderer gegen SEIN DEM an, und ein
        # Objektiv, das dieses DEM begraebt, wird auf die Mindestfreiheit gehoben. Der Hub steht
        # danach auf der Seite. Ein Baum, dessen Krone das Auge enthaelt, faellt im Renderer weg.
        scene = OUT / f"{slug}-scene.json"
        scene.write_text(json.dumps({
            "lat": c["lat"], "lon": c["lon"], "eyeM": 2.0,
            "yawDeg": c["yawDeg"], "pitchDeg": c["pitchDeg"], "fovDeg": c["fovDeg"],
            "utc": shown.strftime("%Y-%m-%dT%H:%M:%SZ"),
            "windDeg": 250, "windMs": 4.0, "cloudCover": 0.0}, indent=2))

        r = subprocess.run(
            [binp, "--scene", str(scene), "--out", str(shot), "--warm", str(args.warm),
             "--eye-asl", f"{float(c['altM']):.2f}", "--size", f"{W}x{H}"],
            capture_output=True, text=True, cwd=str(SIM))
        sun, eye, lift, cleared, roof = "", 0.0, 0.0, 0, False
        for l in (r.stdout + r.stderr).splitlines():
            if "sunElDeg=" in l and "irradiance" in l:
                sun = l.split("sunElDeg=")[1].split()[0][:5]
            if " standpoint " in l:
                eye = float(l.split("eyeM=")[1].split()[0])
                lift = float(l.split("liftM=")[1].split()[0])
            if " standpoint_roof " in l:
                eye = float(l.split("eyeM=")[1].split()[0])
                lift = float(l.split("totalLiftM=")[1].split()[0])
                roof = True
            if " stand_cleared " in l:
                cleared = int(float(l.split("trees=")[1].split()[0]))
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
                 "--eye-asl", f"{float(c['altM']):.2f}", "--size", f"{W}x{H}"],
                capture_output=True, text=True, cwd=str(SIM))
        ok = shot.exists() and shot.stat().st_size > 1000
        marks = []
        if fog: marks.append("in Wolken")
        if lift > 0.05:
            marks.append(f"{lift:.1f} m gehoben ({'Dach' if roof else 'DEM'} ueber dem Objektiv)")
        if cleared: marks.append(f"{cleared} Baum/Baeume am Auge entfernt")
        if sub: marks.append(sub)
        print(f"{slug:16s} {n:8d} {shown.strftime('%Y-%m-%d %H:%M'):>17s} {sun:>7s}  "
              f"Auge {eye:5.1f} m  {'ok' if ok else 'FEHLT'} {'; '.join(marks)}")
        rows.append((c, when, shown, ok, fog, eye, lift, cleared, sub, roof))
        if ok:
            state[slug] = {"shown": shown.isoformat(), "fog": fog, "eye": eye, "lift": lift,
                           "cleared": cleared, "roof": roof}

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
    for c, when, shown, ok, fog, eye, lift, cleared, sub, roof in rows:
        if not ok:
            continue
        fit = (f"Klaffen {c['residPx']} px" if c.get("fitted")
               else f"<span class=w>Pose ungefittet ({c.get('residPx', '?')} px)</span>")
        if fog: fit += " <span class=w>in Wolken</span>"
        ts = shown.strftime("%Y-%m-%d %H:%M UTC") if shown else "&mdash;"
        if sub:
            ts += f" <span class=w>&mdash; {sub}, live {when:%H:%M} UTC</span>"
        pair2 = ""
        if (OUT / f"{c['slug']}-fit.png").exists():
            pair2 = (f"<div class=p><img src='{c['slug']}-fit.jpg'>"
                     f"<img src='{c['slug']}-fit.png'></div>")
        html.append(f"<div class=c><div class=t><b>{c['name']}</b>"
                    f"<span>{ts} &middot; {fit}</span></div>"
                    f"<div class=p><img src='{c['slug']}-live.jpg'>"
                    f"<img src='{c['slug']}-outshine.png'></div>{pair2}"
                    f"<div class=n>{c['altM']} m ueber NN, {eye:.1f} m ueber Grund"
                    + (f" <span class=w>(um {lift:.1f} m gehoben, "
                       f"{'das Dach' if roof else 'das DEM'} liegt ueber dem Objektiv)</span>"
                       if lift > 0.05 else "")
                    + (f" <span class=w>&middot; {cleared} Baum/Baeume am Auge entfernt</span>"
                       if cleared else "")
                    + " &middot; "
                    f"Azimut {c['yawDeg']:.1f}&deg;, Neigung {c['pitchDeg']:.1f}&deg;, "
                    f"Bildwinkel {c['fovDeg']:.1f}&deg;"
                    + (f" &middot; Einpassbild {c['fitImage']} Ortszeit" if c.get("fitImage") else "")
                    + "</div></div>")
    html.append("</div>")
    dead = [c["name"] for c, _w, _s, ok, _f, _e, _l, _cl, _sub, _r in rows if not ok]
    if dead:
        html.append(f"<p class=n>Ohne Livebild und darum nicht gerendert: {', '.join(dead)}</p>")
    statef.write_text(json.dumps(state, indent=2, sort_keys=True))
    (OUT / "index.html").write_text("\n".join(html))
    print(f"\n{OUT/'index.html'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
