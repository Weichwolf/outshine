#!/usr/bin/env python3
"""Azimut und Bildwinkel einer festen Kamera aus dem HORIZONT bestimmen.

  tools/posefit.py --bin build/gpu_walk --cam lech --date 2026/08/05 --time 1200

Die Kamera veroeffentlicht Standort und Hoehe, aber weder Azimut noch Bildwinkel, und die Bilder
tragen kein EXIF. Beides steckt jedoch im Bild: der Grat gegen den Himmel ist ein eindimensionales
Signal, und der DEM sagt, wie er von diesem Punkt aus aussehen MUSS.

Damit die Suche nicht hunderte Renderlaeufe kostet, wird die Skyline EINMAL rundum aufgenommen
(sechs Bilder zu 60 Grad) und danach nur noch verschoben und gestaucht — eine 1D-Korrelation.

Das Restklaffen ist die Aufnahmepruefung: laesst sich der Grat durch keine Pose zur Deckung
bringen, kommt die Kamera nicht ins Feld, sonst wandert unser eigener Fehler in die Referenz.
"""
import argparse, json, math, os, pathlib, subprocess, sys, tempfile, urllib.request

SIM = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(SIM / "tools"))
BASE = "https://www.foto-webcam.eu/webcam"
UA = {"User-Agent": "Mozilla/5.0 (outshine reference harness)"}
PANO_FOV = 60.0
PANO_N = 6
COLS = 720            # Spalten der Rundum-Skyline: 0.5 Grad je Spalte


def read_png(path):
    import struct, zlib
    d = open(path, "rb").read()
    i = 8; w = h = ct = 0; idat = b""
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
    return w, h, bpp, bytes(out)


def skyline(w, h, bpp, px, top_skip=0):
    """Je Spalte die erste Zeile von oben, die nicht mehr Himmel ist, als Anteil der Bildhoehe.

    Himmel ist BLAU ODER HELL UND FARBLOS — eine weisse Wolke ist nicht blau, und ein Test allein auf
    blaue Dominanz legt die Skyline auf ihre Oberkante statt auf den Grat. Firn ist ebenfalls hell und
    farblos, aber er steht unter dem Grat und nicht ueber ihm, deshalb hier ungefaehrlich."""
    out = []
    for x in range(w):
        y = top_skip
        while y < h:
            i = (y*w + x)*bpp
            r, g, b = px[i], px[i+1], px[i+2]
            blue = b > r + 6 and b > 40
            mx, mn = max(r, g, b), min(r, g, b)
            pale = mx > 150 and (mx - mn) < 0.14 * mx
            if not (blue or pale):
                break
            y += 1
        out.append(y / h)
    return out


def render(binp, scene, out, yaw, fov, warm, size="640x360"):
    s = json.loads(pathlib.Path(scene).read_text())
    s["yawDeg"] = yaw; s["fovDeg"] = fov; s["pitchDeg"] = 0
    tmp = tempfile.mktemp(suffix=".json")
    pathlib.Path(tmp).write_text(json.dumps(s))
    subprocess.run([binp, "--scene", tmp, "--out", out, "--warm", str(warm),
                    "--eye", str(s.get("_eyeAgl", 3.0)), "--size", size],
                   capture_output=True, text=True, cwd=str(SIM))
    os.unlink(tmp)
    return os.path.exists(out) and os.path.getsize(out) > 1000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default="build/gpu_walk")
    ap.add_argument("--cam", required=True)
    ap.add_argument("--date", required=True, help="JJJJ/MM/TT")
    ap.add_argument("--time", required=True, help="hhmm UTC+2 der Kamera")
    ap.add_argument("--warm", type=int, default=20000)
    ap.add_argument("--work", default="/tmp/posefit")
    args = ap.parse_args()

    cams = json.loads((SIM / "assets/world/webcams.json").read_text())["cams"]
    cam = next((c for c in cams if c["slug"] == args.cam), None)
    if not cam: sys.exit(f"unbekannte Kamera {args.cam}")
    work = pathlib.Path(args.work); work.mkdir(parents=True, exist_ok=True)
    binp = os.path.abspath(args.bin)

    photo = work / f"{args.cam}-ref.jpg"
    url = f"{BASE}/{args.cam}/{args.date}/{args.time}_hd.jpg"
    d = urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60).read()
    photo.write_bytes(d)
    png = work / f"{args.cam}-ref.png"
    subprocess.run(["sips", "-s", "format", "png", "-Z", "1280", str(photo), "--out", str(png)],
                   capture_output=True)
    pw, ph, pbpp, ppx = read_png(png)
    ref = skyline(pw, ph, pbpp, ppx, top_skip=int(ph*0.04))   # die Einblendung oben ueberspringen
    print(f"Referenz {url}  {pw}x{ph}  Horizont p50 {sorted(ref)[len(ref)//2]:.3f}")

    scene = work / f"{args.cam}-scene.json"
    scene.write_text(json.dumps({
        "lat": cam["lat"], "lon": cam["lon"], "eyeM": 3.0, "yawDeg": 0, "pitchDeg": 0,
        "fovDeg": PANO_FOV, "utc": "2026-06-21T10:00:00Z",
        "windDeg": 250, "windMs": 2.0, "cloudCover": 0.0, "_eyeAgl": 3.0}, indent=2))

    pano = []
    for k in range(PANO_N):
        yaw = k * (360.0 / PANO_N)
        p = str(work / f"{args.cam}-pano{k}.png")
        if not render(binp, scene, p, yaw, PANO_FOV, args.warm):
            sys.exit(f"Panoramabild {k} fehlt")
        w, h, bpp, px = read_png(p)
        sl = skyline(w, h, bpp, px)
        pano.append((yaw, sl))
        print(f"  Panorama {yaw:5.1f} Grad  Horizont p50 {sorted(sl)[len(sl)//2]:.3f}")

    # Rundumprofil: Spalte -> Azimut, linear im Bildwinkel (kleiner Fehler bei 60 Grad, unter 1 Grad)
    ring = [None]*COLS
    for yaw, sl in pano:
        n = len(sl)
        for x, v in enumerate(sl):
            az = yaw - PANO_FOV/2 + PANO_FOV*(x + 0.5)/n
            ring[int(az/360.0*COLS) % COLS] = v
    for i in range(COLS):
        if ring[i] is None: ring[i] = ring[i-1]

    best = None
    # Ein Webcamobjektiv liegt zwischen Weitwinkel und leichtem Tele. Ohne diese Schranke laeuft die
    # Anpassung an den unteren Rand: ein schmaler Bildwinkel findet im Rundumprofil immer ein zufaellig
    # passendes Stueck, und das Restklaffen sagt dann nichts mehr ueber die Pose.
    for fov in [x*0.5 for x in range(70, 181)]:            # 35 bis 90 Grad
        span = fov/360.0*COLS
        for yaw10 in range(0, 3600):
            c0 = yaw10/10.0/360.0*COLS - span/2
            # Auf die FORM passen, nicht auf die Lage: die Neigung der Kamera ist unbekannt und
            # verschiebt beide Profile gegeneinander senkrecht. Ohne diesen Abzug gleicht der
            # Optimierer die Verschiebung aus, indem er den Bildwinkel schrumpft, und laeuft an den
            # unteren Rand des Suchbereichs — gemessen dreimal hintereinander.
            us = []; th = []
            for j in range(0, len(ref), 8):
                a = c0 + span*(j + 0.5)/len(ref)
                us.append(ring[int(a) % COLS]); th.append(ref[j])
            du = sum(us)/len(us); dt = sum(th)/len(th)
            err = sum(abs((u - du) - (t - dt)) for u, t in zip(us, th)) / len(us)
            if best is None or err < best[0]:
                best = (err, yaw10/10.0, fov, du - dt)
    err, yaw, fov, dy = best
    pitch = math.degrees(math.atan(2.0 * dy * math.tan(math.radians(fov/2.0))))
    print(f"\nBESTE POSE  Azimut {yaw:.1f} Grad   Bildwinkel {fov:.1f} Grad   Neigung {pitch:+.1f} Grad")
    print(f"Restklaffen der FORM {err:.4f} der Bildhoehe = {err*ph:.1f} px")
    print("Aufnahme ins Feld nur, wenn das Restklaffen klein gegen die Gratamplitude ist.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
