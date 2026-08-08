#!/usr/bin/env python3
"""Resektion einer festen Webcam: Azimut, Neigung, Rollung und Bildwinkel aus dem DEM-Horizont.

  tools/campose.py --cam hochries              # eine Kamera einpassen
  tools/campose.py --all --write               # alle einpassen und cams.json fortschreiben

Der Betreiber veroeffentlicht Standort, Objektivhoehe, Blickrichtung und Brennweite; er
veroeffentlicht NICHT den Sensor, also auch nicht den Bildwinkel, und die Bilder tragen kein EXIF.
Der Rest ist Resektion: der Kachelserver liefert DEM und (unter /peaks) die benannten OSM-Gipfel,
und daraus steht fest, wie der Grat von diesem Punkt aus aussehen MUSS. Gemessen wird die
Silhouette im Bild, gesucht die Pose, die beides zur Deckung bringt.

Das getrimmte Restklaffen in Pixeln ist die Aufnahmepruefung.
"""
import argparse, datetime, json, math, os, pathlib, sys, time, urllib.request
import numpy as np
from PIL import Image, ImageDraw

SIM = pathlib.Path(__file__).resolve().parent.parent
CAMS = SIM.parent / "mods/webcams/cams.json"
TILES = os.environ.get("FB_TILES", "http://localhost:8081")
WORK = pathlib.Path(os.environ.get("CAMPOSE_WORK", "/tmp/campose"))
UA = {"User-Agent": "Mozilla/5.0 (outshine reference harness)"}
FOTO = "https://www.foto-webcam.eu/webcam"

DEM_Z = 12                 # 25.9 m/px bei 47 Grad Nord = die native Aufloesung der 1-Bogensekunden-Quelle
RANGE_M = 50000.0
RAY_STEP_M = 25.0
K_REFR = 0.13
R_EARTH = 6371008.8

# Sensorbreite in mm. Der Betreiber nennt die Brennweite, nicht den Sensor, also ist das die eine
# Zahl, die der Bildwinkel noch braucht. GEMESSEN mit --scan: bei den drei Kameras, deren Silhouette
# ueberhaupt zur Deckung kommt, liegt das Minimum des Restklaffens ueber der Sensorbreite bei
# nebelhorn 22 mm (2,09 px), herzogstand 21-22 mm (2,88 px) und innsbruck 21-25 mm (2,85 px); der
# Gegentest bei 36 mm setzt den Hochvogel im Nebelhornbild um 300 px neben seine Spitze.
SENSOR_MM = 22.3


def http(url, timeout=120):
    return urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout).read()


def m_per_deg(lat):
    p = math.radians(lat)
    return (111132.92 - 559.82 * math.cos(2 * p) + 1.175 * math.cos(4 * p),
            111412.84 * math.cos(p) - 93.5 * math.cos(3 * p))


def fov_from_mm(mm, sensor_mm):
    return 2.0 * math.degrees(math.atan(sensor_mm / (2.0 * mm)))


class Dem:
    """Terrarium-Kacheln vom Kachelserver, zu EINEM Feld in Mercator-Pixeln zusammengesetzt."""

    def __init__(self, lat, lon, radius_m, z=DEM_Z):
        self.z = z
        n = 1 << z
        mlat, mlon = m_per_deg(lat)
        dlat, dlon = radius_m / mlat, radius_m / mlon
        x0, y1 = self._px(lat - dlat, lon - dlon)
        x1, y0 = self._px(lat + dlat, lon + dlon)
        self.tx0, self.ty0 = int(x0 // 256), int(y0 // 256)
        tx1, ty1 = int(x1 // 256), int(y1 // 256)
        self.a = np.full(((ty1 - self.ty0 + 1) * 256, (tx1 - self.tx0 + 1) * 256), np.nan, np.float32)
        self.tiles = self.miss = 0
        cache = WORK / "dem"
        cache.mkdir(parents=True, exist_ok=True)
        for ty in range(self.ty0, ty1 + 1):
            for tx in range(self.tx0, tx1 + 1):
                if not (0 <= tx < n and 0 <= ty < n):
                    continue
                self.tiles += 1
                p = cache / f"{z}_{tx}_{ty}.png"
                if not p.exists():
                    body = None
                    for k in range(40):
                        try:
                            r = urllib.request.urlopen(f"{TILES}/t/terrain/{z}/{tx}/{ty}", timeout=60)
                            if r.status == 200:
                                body = r.read()
                                break
                        except Exception:
                            pass
                        time.sleep(0.5 if k < 20 else 2.0)
                    if body is None:
                        self.miss += 1
                        continue
                    p.write_bytes(body)
                rgb = np.asarray(Image.open(p).convert("RGB"), np.float32)
                e = rgb[:, :, 0] * 256.0 + rgb[:, :, 1] + rgb[:, :, 2] / 256.0 - 32768.0
                iy, ix = (ty - self.ty0) * 256, (tx - self.tx0) * 256
                self.a[iy:iy + 256, ix:ix + 256] = e

    def _px(self, lat, lon):
        n = float(1 << self.z) * 256.0
        s = math.sin(math.radians(max(-85.0, min(85.0, lat))))
        return ((lon + 180.0) / 360.0 * n,
                (0.5 - math.log((1 + s) / (1 - s)) / (4 * math.pi)) * n)

    def sample(self, lat, lon):
        n = float(1 << self.z) * 256.0
        x = (lon + 180.0) / 360.0 * n - self.tx0 * 256
        s = np.sin(np.radians(np.clip(lat, -85.0, 85.0)))
        y = (0.5 - np.log((1 + s) / (1 - s)) / (4 * np.pi)) * n - self.ty0 * 256
        x = np.clip(x, 0, self.a.shape[1] - 1.001)
        y = np.clip(y, 0, self.a.shape[0] - 1.001)
        x0 = x.astype(np.int32); y0 = y.astype(np.int32)
        fx = (x - x0).astype(np.float32); fy = (y - y0).astype(np.float32)
        a = self.a
        return (a[y0, x0] * (1 - fx) + a[y0, x0 + 1] * fx) * (1 - fy) + \
               (a[y0 + 1, x0] * (1 - fx) + a[y0 + 1, x0 + 1] * fx) * fy


def drop_m(dist):
    """Erdkruemmung abzueglich Refraktion: der scheinbare Hoehenabfall auf der Entfernung."""
    return dist * dist * (1.0 - K_REFR) / (2.0 * R_EARTH)


def horizon(dem, lat, lon, eye_m, az_deg, rmax=RANGE_M, step=RAY_STEP_M):
    mlat, mlon = m_per_deg(lat)
    r = np.arange(step, rmax, step, dtype=np.float32)
    d = drop_m(r)
    out = np.empty(len(az_deg), np.float32)
    for i0 in range(0, len(az_deg), 128):
        az = np.radians(az_deg[i0:i0 + 128])[:, None]
        h = dem.sample(lat + np.cos(az) * r / mlat, lon + np.sin(az) * r / mlon)
        ang = np.degrees(np.arctan2(h - eye_m - d, r))
        out[i0:i0 + 128] = np.nanmax(np.where(np.isnan(h), -90.0, ang), axis=1)
    return out


def peak_azel(lat, lon, eye_m, pk):
    mlat, mlon = m_per_deg(lat)
    dx = (pk[:, 1] - lon) * mlon
    dy = (pk[:, 0] - lat) * mlat
    d = np.hypot(dx, dy)
    return (np.degrees(np.arctan2(dx, dy)) % 360.0,
            np.degrees(np.arctan2(pk[:, 2] - eye_m - drop_m(d), d)), d)


def load_peaks(lat, lon, r=RANGE_M):
    p = WORK / f"peaks_{lat:.4f}_{lon:.4f}_{int(r)}.tsv"
    if not p.exists():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(http(f"{TILES}/peaks?lat={lat:.6f}&lon={lon:.6f}&r={int(r)}", 900))
    rows, names = [], []
    for line in p.read_text("utf-8", "replace").splitlines():
        f = line.split("\t")
        if len(f) < 4:
            continue
        try:
            ele = float(f[2].split()[0].replace(",", "."))
        except (ValueError, IndexError):
            continue
        if not -500.0 < ele < 9000.0:
            continue
        rows.append((float(f[0]), float(f[1]), ele)); names.append(f[3])
    return np.array(rows, np.float64).reshape(-1, 3), names


def basis(yaw, pitch, roll):
    y, p, rr = math.radians(yaw), math.radians(pitch), math.radians(roll)
    f = np.array([math.sin(y) * math.cos(p), math.cos(y) * math.cos(p), math.sin(p)])
    r0 = np.array([math.cos(y), -math.sin(y), 0.0])
    u0 = np.cross(r0, f)
    cr, sr = math.cos(rr), math.sin(rr)
    return f, r0 * cr + u0 * sr, u0 * cr - r0 * sr


def project(az, el, pose, w, h):
    yaw, pitch, roll, fovh = pose
    fpx = (w / 2.0) / math.tan(math.radians(fovh) / 2.0)
    a, e = np.radians(az), np.radians(el)
    d = np.stack([np.sin(a) * np.cos(e), np.cos(a) * np.cos(e), np.sin(e)], -1)
    f, r, u = basis(yaw, pitch, roll)
    zc = np.maximum(d @ f, 1e-3)
    return fpx * (d @ r) / zc + w / 2.0, h / 2.0 - fpx * (d @ u) / zc, d @ f


def _dp_path(U, lam):
    h, w = U.shape
    dp = U[:, 0].copy(); back = np.zeros((w, h), np.int32); idx = np.arange(h)
    for x in range(1, w):
        f = dp.copy(); arg = idx.copy()
        for i in range(1, h):
            if f[i - 1] + lam < f[i]: f[i] = f[i - 1] + lam; arg[i] = arg[i - 1]
        for i in range(h - 2, -1, -1):
            if f[i + 1] + lam < f[i]: f[i] = f[i + 1] + lam; arg[i] = arg[i + 1]
        back[x] = arg; dp = f + U[:, x]
    y = int(np.argmin(dp)); path = np.zeros(w, np.int32)
    for x in range(w - 1, -1, -1):
        path[x] = y
        if x: y = back[x][y]
    return path


def skyline(im, scale=2, delta=16.0, lam=30.0, rounds=6):
    """Himmel/Gelaende als kuerzester Weg, mit dem Himmelsmodell im Wechsel neu geschaetzt.

    Zwei Fallen liegen hier: erstens ist der Himmel oben dunkelblau und ueber dem Grat dunstig hell,
    ein fester Schwellwert taugt also nicht -- das Modell ist eine Flaeche ueber (x, y), und es wird
    NUR auf dem bereits als Himmel erkannten Teil neu gefittet (EM). Zweitens sind Wolken HELLER als
    der Himmel: das Kriterium ist vorzeichenbehaftet, sonst legt der Weg sich auf die Wolkenkante."""
    a = np.asarray(im.convert("RGB").resize((im.size[0] // scale, im.size[1] // scale),
                                            Image.BILINEAR), np.float32)
    h, w, _ = a.shape
    lum = 0.2126 * a[:, :, 0] + 0.7152 * a[:, :, 1] + 0.0722 * a[:, :, 2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32); yy /= h; xx /= w
    b = np.stack([np.ones_like(yy), yy, yy * yy, xx, xx * yy, xx * xx], -1).reshape(-1, 6)
    rows = np.arange(h)[:, None]
    path = np.full(w, h // 8, np.int32)
    zero = np.zeros((1, w), np.float32)
    for _ in range(rounds):
        m = (rows < path[None, :]).reshape(-1)
        if m.sum() < 800:
            break
        coef = np.linalg.lstsq(b[m], lum.reshape(-1)[m], rcond=None)[0]
        g = lum - (b @ coef).reshape(h, w).astype(np.float32)
        ca = np.cumsum(np.vstack([zero, np.maximum(0.0, -g - delta)]), 0)
        cb = np.cumsum(np.vstack([zero, np.minimum(np.maximum(0.0, g + delta), 3.0 * delta)]), 0)
        new = _dp_path(ca[:h] + (cb[h][None, :] - cb[:h]), lam)
        done = np.abs(new - path).mean() < 0.5
        path = new
        if done:
            break
    return np.interp(np.arange(im.size[0]), (np.arange(w) + 0.5) * scale,
                     path.astype(np.float32) * scale)


def nelder_mead(f, x0, step, iters=500):
    n = len(x0)
    s = [x0] + [x0 + step * np.eye(n)[i] for i in range(n)]
    v = [f(x) for x in s]
    for _ in range(iters):
        o = np.argsort(v); s = [s[i] for i in o]; v = [v[i] for i in o]
        c = np.mean(s[:-1], axis=0)
        xr = c + (c - s[-1]); fr = f(xr)
        if fr < v[0]:
            xe = c + 2 * (c - s[-1]); fe = f(xe)
            s[-1], v[-1] = (xe, fe) if fe < fr else (xr, fr)
        elif fr < v[-2]:
            s[-1], v[-1] = xr, fr
        else:
            xc = c + 0.5 * (s[-1] - c); fc = f(xc)
            if fc < v[-1]:
                s[-1], v[-1] = xc, fc
            else:
                for i in range(1, n + 1):
                    s[i] = s[0] + 0.5 * (s[i] - s[0]); v[i] = f(s[i])
    return s[int(np.argmin(v))]


class Fit:
    """Eine Aufnahme: die gemessene Silhouette gegen den projizierten DEM-Horizont."""

    def __init__(self, sl, az_h, el_h, w, h, stride=6):
        self.cols = np.arange(0, w, stride)
        self.meas = sl[self.cols]
        self.az, self.el, self.w, self.h = az_h, el_h, w, h

    def resid(self, pose):
        x, y, zc = project(self.az, self.el, pose, self.w, self.h)
        m = (zc > 0.2) & (x > -self.w) & (x < 2 * self.w)
        if m.sum() < 40:
            return None
        o = np.argsort(x[m])
        r = np.interp(self.cols, x[m][o], y[m][o], left=np.nan, right=np.nan) - self.meas
        return r[~np.isnan(r)]

    def loss(self, pose, trim=0.7):
        r = self.resid(pose)
        if r is None or len(r) < 0.5 * len(self.cols):
            return 1e6
        a = np.sort(np.abs(r))
        return float(a[:max(8, int(trim * len(a)))].mean())


class Pose:
    """EINE Pose gegen MEHRERE Aufnahmen derselben Kamera.

    Der Bildwinkel ist KEIN Suchparameter. Er folgt aus der veroeffentlichten Brennweite und der
    gemessenen Sensorbreite, weil das Restklaffen ueber dem Bildwinkel bei den meisten Kameras flach
    ist: gesucht wuerde dann der Rand des Suchbereichs, nicht das Optimum. Frei bleiben Azimut
    (eng um die veroeffentlichte Blickrichtung), Neigung und Rollung -- ein Versatz, eine Kippung."""

    YAW_WIN = 4.0

    def __init__(self, fits, yaw0, fov):
        self.fits, self.yaw0, self.fov = fits, yaw0, fov
        self.keep = max(3, len(fits) // 2)

    def each(self, p):
        return [f.loss((p[0], p[1], p[2], self.fov)) for f in self.fits]

    def loss(self, p):
        pen = max(0.0, abs(p[2]) - 6.0) ** 2 * 50.0 \
            + max(0.0, abs(p[0] - self.yaw0) - self.YAW_WIN) ** 2 * 50.0
        return float(np.mean(sorted(self.each(p))[:self.keep])) + pen

    def coarse(self):
        best = None
        for yaw in np.arange(self.yaw0 - self.YAW_WIN, self.yaw0 + self.YAW_WIN + 0.01, 0.2):
            es, dys = [], []
            for f in self.fits:
                r = f.resid((yaw, 0.0, 0.0, self.fov))
                if r is None or len(r) < 0.5 * len(f.cols):
                    continue
                dy = float(np.median(r))
                a = np.sort(np.abs(r - dy))
                es.append(float(a[:max(8, int(0.7 * len(a)))].mean())); dys.append(dy)
            if len(es) < self.keep:
                continue
            o = np.argsort(es)[:self.keep]
            e = float(np.mean([es[i] for i in o]))
            if best is None or e < best[0]:
                best = (e, yaw, float(np.median([dys[i] for i in o])))
        return best

    def solve(self, w):
        b = self.coarse()
        if b is None:
            return None
        fpx = (w / 2.0) / math.tan(math.radians(self.fov) / 2.0)
        p = nelder_mead(self.loss, np.array([b[1], math.degrees(math.atan(b[2] / fpx)), 0.0]),
                        np.array([0.4, 0.4, 0.4]), 400)
        return (float(p[0]), float(p[1]), float(p[2]), self.fov), self.loss(p)


def overlay(img, sl, az_h, el_h, pose, paz, pel, names, dst):
    im = img.convert("RGB").copy()
    d = ImageDraw.Draw(im)
    w, h = im.size
    for x in range(0, w, 3):
        if not np.isnan(sl[x]):
            d.rectangle([x, int(sl[x]) - 1, x + 1, int(sl[x]) + 1], fill=(255, 60, 60))
    x, y, zc = project(az_h, el_h, pose, w, h)
    pts = [(float(a), float(b)) for a, b, k in zip(x, y, zc) if k > 0.2 and -w < a < 2 * w]
    if len(pts) > 1:
        d.line(pts, fill=(80, 200, 255), width=2)
    px, py, pz = project(paz, pel, pose, w, h)
    for i, nm in enumerate(names):
        if pz[i] > 0.2 and 0 <= px[i] < w and -40 < py[i] < h:
            d.line([(px[i], py[i] - 16), (px[i], py[i] + 16)], fill=(255, 220, 0), width=1)
            d.text((px[i] + 3, py[i] - 26), nm, fill=(255, 220, 0))
    im.save(dst)


def clarity(path):
    """Dunst und Wolke schlucken die Silhouette. Buntheit mal Kontrast trennt beides zuverlaessig."""
    a = np.asarray(Image.open(path).convert("RGB").resize((256, 144)), np.float32)
    mx, mn = a.max(2), a.min(2)
    sat = float(((mx - mn) / np.maximum(mx, 1.0)).mean())
    lum = 0.2126 * a[:, :, 0] + 0.7152 * a[:, :, 1] + 0.0722 * a[:, :, 2]
    return sat * float(np.percentile(lum, 95) - np.percentile(lum, 5)), sat


def clear_images(slug, days, work, n, hours=(9, 11, 13, 15, 17)):
    now = datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=2)))
    out = []
    for dd in range(days):
        day = now - datetime.timedelta(days=dd)
        for hh in hours:
            if dd == 0 and hh > now.hour:
                continue
            c = f"{day:%Y/%m/%d}/{hh:02d}00"
            p = work / ("s_" + c.replace("/", "") + ".jpg")
            if not p.exists():
                try:
                    p.write_bytes(http(f"{FOTO}/{slug}/{c}_hd.jpg", 60))
                except Exception:
                    p.write_bytes(b"")
            if p.stat().st_size < 10000:
                continue
            score, sat = clarity(p)
            out.append((score, c, p, sat))
    out.sort(key=lambda t: -t[0])
    return out[:n]


def prepare(c, shots):
    dem = Dem(c["lat"], c["lon"], RANGE_M)
    ground = float(dem.sample(np.array([c["lat"]]), np.array([c["lon"]]))[0])
    span = fov_from_mm(float(c["fovMm"]), 40.0) + 20.0
    az_h = np.arange(c["dirDeg"] - span / 2, c["dirDeg"] + span / 2, 0.02)
    el_h = horizon(dem, c["lat"], c["lon"], float(c["altM"]), az_h)
    fits, shown = [], []
    for _, when, path, sat in shots:
        img = Image.open(path)
        sl = skyline(img)
        if not (0.03 * img.size[1] < np.median(sl) < 0.88 * img.size[1]):
            continue                       # Weg am Bildrand: Nebel, oder ein Blick ohne Himmel
        fits.append(Fit(sl, az_h, el_h, img.size[0], img.size[1]))
        shown.append((when, img, sat, sl))
    return dem, ground, az_h, el_h, fits, shown


def unfitted(c):
    """Eine gescheiterte Einpassung hinterlaesst keine Zahl aus einem frueheren Lauf."""
    c["yawDeg"], c["pitchDeg"], c["rollDeg"] = c["dirDeg"], 0.0, 0.0
    c["fitted"] = False
    c.pop("residPx", None); c.pop("fitImage", None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cam", default="")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--scan", action="store_true",
                    help="Restklaffen ueber der Sensorbreite -- so wurde SENSOR_MM gemessen")
    ap.add_argument("--days", type=int, default=14)
    ap.add_argument("--shots", type=int, default=10)
    ap.add_argument("--sensor", type=float, default=SENSOR_MM)
    ap.add_argument("--accept", type=float, default=10.0, help="Restklaffen-Schranke in Pixeln")
    args = ap.parse_args()

    doc = json.loads(CAMS.read_text())
    cams = doc["cams"]
    if args.cam:
        want = args.cam.split(",")
        cams = [c for c in cams if c["slug"] in want]
    elif not (args.all or args.scan):
        sys.exit("--cam, --all oder --scan")

    WORK.mkdir(parents=True, exist_ok=True)
    if args.scan:
        prep = {}
        for c in cams:
            work = WORK / c["slug"]; work.mkdir(parents=True, exist_ok=True)
            prep[c["slug"]] = prepare(c, clear_images(c["slug"], args.days, work, args.shots))
        print(f"{'Sensor':>7s} " + " ".join(f"{c['slug'][:9]:>9s}" for c in cams))
        for sensor in np.arange(14.0, 40.1, 1.0):
            row = []
            for c in cams:
                _g, _gr, _a, _e, fits, shown = prep[c["slug"]]
                if not fits:
                    row.append(float("nan")); continue
                r = Pose(fits, c["dirDeg"], fov_from_mm(float(c["fovMm"]), sensor)).solve(shown[0][1].size[0])
                row.append(r[1] if r else float("nan"))
            print(f"{sensor:7.1f} " + " ".join(f"{v:9.2f}" for v in row))
        return 0

    for c in cams:
        work = WORK / c["slug"]; work.mkdir(parents=True, exist_ok=True)
        lat, lon, eye = c["lat"], c["lon"], float(c["altM"])
        shots = clear_images(c["slug"], args.days, work, args.shots)
        if not shots:
            print(f"{c['slug']:12s} kein Archivbild"); unfitted(c); continue
        dem, ground, az_h, el_h, fits, shown = prepare(c, shots)
        if not fits:
            print(f"{c['slug']:12s} keine Silhouette in {len(shots)} Aufnahmen")
            unfitted(c); continue

        mm = float(c["fovMm"])
        fov = fov_from_mm(mm, args.sensor)
        w, h = shown[0][1].size
        ps = Pose(fits, c["dirDeg"], fov)
        got = ps.solve(w)
        if got is None:
            print(f"{c['slug']:12s} keine Grobloesung"); unfitted(c); continue
        pose, resid = got
        yaw, pitch, roll, _ = pose
        per = ps.each(pose[:3])
        k = int(np.argmin(per))
        when, img, sat, sl = shown[k]

        pk, names = load_peaks(lat, lon)
        paz, pel, pd = peak_azel(lat, lon, eye, pk)
        keep = (pd > 2000) & (pel >= np.interp(paz, az_h % 360.0, el_h, period=360.0) - 0.03)
        paz, pel = paz[keep], pel[keep]
        names = [n for n, kk in zip(names, keep) if kk]
        px, _py, pz = project(paz, pel, pose, w, h)
        seen = int(((pz > 0.2) & (px >= 0) & (px < w)).sum())

        overlay(img, sl, az_h, el_h, pose, paz, pel, names, work / "overlay.png")
        ok = (resid <= args.accept and abs(roll) <= 6.0 and dem.miss == 0
              and abs(yaw - c["dirDeg"]) <= ps.YAW_WIN)
        print(f"{c['slug']:12s} Boden {ground:7.1f}  Auge {eye - ground:+6.1f} m  {when} "
              f"Bunt {sat:.2f}  DEM {dem.tiles - dem.miss:3d}/{dem.tiles}  Gipfel {seen:3d}  "
              f"Az {yaw:7.2f} ({c['dirDeg']:5.1f} gemeldet)  Neig {pitch:+6.2f}  Roll {roll:+5.2f}  "
              f"Bildwinkel {fov:5.2f}  Klaffen {resid:6.2f} px "
              f"[{' '.join(f'{v:.1f}' for v in sorted(per))}]  {'JA' if ok else 'NEIN'}")
        c["yawDeg"], c["pitchDeg"], c["rollDeg"], c["fovDeg"] = (
            round(yaw, 2), round(pitch, 2), round(roll, 2), round(fov, 2))
        c["groundM"] = round(ground, 1)
        c["residPx"] = round(resid, 2)
        c["fitImage"] = when
        c["fitted"] = bool(ok)

    if args.write:
        CAMS.write_text(json.dumps(doc, indent=2, ensure_ascii=False) + "\n")
        print(f"\n{CAMS}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
