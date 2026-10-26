"""Bake the FlightBox star catalogue into incremental magnitude bands for fb-tiles.

Source : HYG v4 (astronexus/HYG-Database, CC-BY-SA 4.0), the Hipparcos-derived open catalogue.
Output : tiles/stars/band{0..3}.bin, 6 bytes/star, sorted by magnitude, little-endian.

Wire format per star (little-endian, `<HhBB`):
    ra   uint16   0..360 deg   -> u/65536*360       (0.0055 deg/step, sub-pixel at any FOV)
    dec  int16   -90..+90 deg  -> i/32767*90        (0.0027 deg/step)
    mag  uint8   -1.5..6.5     -> u/255*8 - 1.5      (0.031 mag/step)
    bv   uint8   -0.5..+2.5    -> u/255*3 - 0.5      (Johnson B-V colour index)

Bands are incremental (non-redundant): the renderer concatenates band 0..N up to the magnitude its
view condition warrants (EVS camera ~band 0, clean SVS ~band 3). Served as /t/stars/{band}/0/0.

Positions are HYG's J2000/ICRS, carried forward to the run epoch: proper motion (pmra/pmdec) in the
J2000 frame, then IAU-1976 precession of the equatorial frame to equinox-of-date -- because GMST in
the renderer is referenced to the equinox of date, and the ~0.36 deg (26 yr) precession is the
dominant term, well above one screen pixel.

Idempotent: same source + same --epoch -> byte-identical output. Re-run to advance the epoch; the
generated .bin are committed and COPYed into the image, so the container build stays offline.
"""
import argparse, csv, datetime as dt, io, math, os, struct, sys, urllib.request

HYG_URL = "https://raw.githubusercontent.com/astronexus/HYG-Database/main/hyg/CURRENT/hygdata_v41.csv"
MAG_MAX = 6.5
BANDS   = [3.0, 4.5, 6.0, 6.5]
ARCSEC  = math.pi / 180.0 / 3600.0
J2000   = 2451545.0

def jd_of_year(year):
    y = int(math.floor(year))
    base = dt.date(y, 1, 1).toordinal()
    days = dt.date(y, 12, 31).toordinal() + 1 - base
    frac_ordinal = base + (year - y) * days
    return frac_ordinal + 1721424.5

def precession_matrix(jd_to):
    """IAU 1976 precession from J2000.0 to jd_to, as a 3x3 rotation of equatorial rectangular
    coordinates. T0 = 0 (fixed start epoch J2000) drops the start-epoch terms."""
    t = (jd_to - J2000) / 36525.0
    zeta  = (2306.2181 * t + 0.30188 * t * t + 0.017998 * t ** 3) * ARCSEC
    z     = (2306.2181 * t + 1.09468 * t * t + 0.018203 * t ** 3) * ARCSEC
    theta = (2004.3109 * t - 0.42665 * t * t - 0.041833 * t ** 3) * ARCSEC

    def rz(a):
        c, s = math.cos(a), math.sin(a)
        return [[c, s, 0.0], [-s, c, 0.0], [0.0, 0.0, 1.0]]

    def ry(a):
        c, s = math.cos(a), math.sin(a)
        return [[c, 0.0, -s], [0.0, 1.0, 0.0], [s, 0.0, c]]

    def mul(a, b):
        return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]

    return mul(rz(-z), mul(ry(theta), rz(-zeta)))

def apply(mat, v):
    return [sum(mat[i][k] * v[k] for k in range(3)) for i in range(3)]

def load_rows(src):
    data = src.read()
    if isinstance(data, bytes):
        data = data.decode("utf-8")
    return list(csv.DictReader(io.StringIO(data)))

def pack(rows, epoch_year):
    jd = jd_of_year(epoch_year)
    dt_yr = epoch_year - 2000.0
    P = precession_matrix(jd)
    out = []
    for r in rows:
        if r.get("proper") == "Sol" or r.get("id") == "0":
            continue
        try:
            mag = float(r["mag"])
        except (ValueError, KeyError):
            continue
        if not mag <= MAG_MAX:
            continue
        ra = float(r["ra"]) * 15.0 * math.pi / 180.0
        dec = float(r["dec"]) * math.pi / 180.0
        pmra = float(r["pmra"] or 0.0) * 1e-3 * ARCSEC
        pmdec = float(r["pmdec"] or 0.0) * 1e-3 * ARCSEC
        cd = math.cos(dec)
        ra += (pmra / cd if cd > 1e-6 else 0.0) * dt_yr
        dec += pmdec * dt_yr
        v = [math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec)]
        x, y, zc = apply(P, v)
        ra_d = math.degrees(math.atan2(y, x)) % 360.0
        dec_d = math.degrees(math.asin(max(-1.0, min(1.0, zc))))
        try:
            bv = float(r["ci"])
        except (ValueError, KeyError):
            bv = 0.0
        out.append((mag, ra_d, dec_d, bv))
    out.sort(key=lambda s: s[0])
    return out

def encode(star):
    mag, ra_d, dec_d, bv = star
    ra_u = round(ra_d / 360.0 * 65536.0) & 0xFFFF
    dec_i = max(-32767, min(32767, round(dec_d / 90.0 * 32767.0)))
    mag_u = max(0, min(255, round((mag + 1.5) / 8.0 * 255.0)))
    bv_u = max(0, min(255, round((bv + 0.5) / 3.0 * 255.0)))
    return struct.pack("<HhBB", ra_u, dec_i, mag_u, bv_u)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", help="local HYG csv (default: download from astronexus)")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "stars"))
    ap.add_argument("--epoch", type=float, default=None,
                    help="equinox/epoch as fractional year (default: now)")
    args = ap.parse_args()

    epoch = args.epoch if args.epoch is not None else (
        dt.datetime.now(dt.timezone.utc).timetuple().tm_yday / 365.25
        + dt.datetime.now(dt.timezone.utc).year)

    if args.src:
        with open(args.src, "r", encoding="utf-8") as f:
            rows = load_rows(f)
    else:
        sys.stderr.write("fetching HYG v41 ...\n")
        with urllib.request.urlopen(HYG_URL, timeout=120) as resp:
            rows = load_rows(resp)

    stars = pack(rows, epoch)
    outdir = os.path.normpath(args.out)
    os.makedirs(outdir, exist_ok=True)

    lo = -99.0
    manifest = ["# FlightBox star catalogue -- generated by build_stars.py",
                "# source: HYG v41 (astronexus/HYG-Database), CC-BY-SA 4.0",
                "# attribution: HYG Database / Hipparcos-derived, CC-BY-SA",
                f"# epoch: {epoch:.3f}   mag_max: {MAG_MAX}   total: {len(stars)}",
                "# format: little-endian <HhBB> per star (ra u16, dec i16, mag u8, bv u8)"]
    total = 0
    for band, hi in enumerate(BANDS):
        sl = [s for s in stars if lo < s[0] <= hi]
        blob = b"".join(encode(s) for s in sl)
        path = os.path.join(outdir, f"band{band}.bin")
        with open(path, "wb") as f:
            f.write(blob)
        manifest.append(f"band{band}: mag ({lo if lo > -99 else '-inf'}, {hi}]  "
                        f"{len(sl)} stars  {len(blob)} B")
        total += len(blob)
        lo = hi
    manifest.append(f"# total served: {total} B across {len(BANDS)} bands")
    with open(os.path.join(outdir, "manifest.txt"), "w") as f:
        f.write("\n".join(manifest) + "\n")
    sys.stderr.write("\n".join(manifest) + "\n")

if __name__ == "__main__":
    main()
