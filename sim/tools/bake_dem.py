#!/usr/bin/env python3
"""FlightBox — bake_dem.py: bakes one lat/lon box of real DEM into a mod's data/<name>.bin for
FBBakedDemElevation (the gym's --elev baked; mod.json's "dem" names the file).

Manual, one-off tool (NOT a build dependency): run it against a LOCAL fb-tiles (default
http://localhost:8081) once; the OUTPUT IS NOT COMMITTED (doc/assets.md §0, "version the recipe,
never the cake") — this file plus the region table below is what a rebuild needs.

    python3 tools/bake_dem.py --region swiss|mekong [--base URL] [--out PATH] [--verify N]

Why the tile route, not point /elev requests: a region raster is 10^7 points — at one HTTP round
trip per point that is hours even parallelized. fb-tiles' public /t/terrain/z/x/y route instead
serves the raw Terrarium-encoded DEM PNGs (elev = R*256+G+B/256-32768, the same mapping tiles/elev.c
and world/FBTerrainField.cpp already decode) that back /elev itself; covering a region's box plus a
1-tile margin is a few thousand fetches, each cached by fb-tiles' own upstream fetch/cache, then one
bilinear resample onto the output grid in-process. --verify N re-asks /elev at N pseudo-random points
inside the box and prints the residual, which is the only end-to-end check that the mosaic, the
Mercator inverse and the grid indexing all agree.

Output format (little-endian, magic "FBDEM01"): see FBBakedDemElevation.cpp's banner for the exact
byte layout this script writes.
"""
import argparse
import hashlib
import json
import math
import os
import random
import struct
import sys
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from io import BytesIO

import numpy as np
from PIL import Image

import fb_mod as mod

MAGIC = b"FBDEM01\0"
BAND_ROWS = 512      # output rows resampled per pass: ~300 MB of working set at the mekong box's width

# One row per baked asset. `zoom` is the Terrarium source zoom, `target_m` the output raster spacing
# at the box's mid-latitude, `edge_blend_m` the width of the smoothstep down to 0 m at the box border
# (an ISLAND contract: right for a fixture that must be surrounded by sea, wrong for a theatre whose
# ground continues past its own box). `geodetic` picks the degree lengths that turn `target_m` into a
# row/column count.
REGIONS = {
    # The f16 fixture. Box [SET, task spec]; zoom 11 is ~52-53 m/px at 46 N, finer than the 90 m grid.
    # geodetic=False: 111 320 m/deg on BOTH axes, which is the equator's LONGITUDE degree applied to a
    # meridian — wrong by +0.14 % at 46.8 N, so the committed raster's rows sit 89.88 m apart and not
    # 90. Kept because it is what the committed swiss-dem-90m.bin was baked with: correcting it turns
    # the raster 3836x2462 into 3836x2459 and moves all 296 f16 mission results, which is a different
    # decision than this one.
    "swiss": dict(mod="f16", out="swiss-dem-90m.bin", zoom=11, target_m=90.0, edge_blend_m=15000.0,
                  geodetic=False,
                  lon_min=5.96, lon_max=10.49, lat_min=45.82, lat_max=47.81),
    # The f22 campaign-one theatre. Box [DERIV]: mods/f22/doc/terrain.md §4's campaign box widened to
    # enclose every lat/lon the eight sorties name (their CAP spawns sit outside it, up to 21.506 N /
    # 102.156 E) plus ~0.15 deg of margin, then rounded outward to 0.05 deg.
    # zoom 13 is what tiles/elev.c's own FB_DEM_Z samples, so the baked surface IS `--elev tiles`'
    # surface and the only remaining difference is the 90 m grid — which --verify measures.
    # No edge blend: this ground continues past the box and a 15 km ramp to sea level would invent a
    # cliff where 1.7's and 1.8's run-ins cross it.
    "mekong": dict(mod="f22", out="mekong-dem-90m.bin", zoom=13, target_m=90.0, edge_blend_m=0.0,
                   geodetic=True,
                   lon_min=98.85, lon_max=102.35, lat_min=17.90, lat_max=21.70),
    # The armored-fist Overwatch theatre: Sindh + western Rajasthan. Box [DERIV]:
    # mods/armored-fist/doc/terrain.md §4's campaign union box widened to enclose every lat/lon the
    # seven sorties name — their run-in spawns sit up to 40 km outside it, and Corrosion's egress
    # crosses the Karachi coast — then rounded outward to 0.05 deg.
    # Same zoom and same no-edge-blend reasoning as mekong; the Indus delta's real 0 m coastline is in
    # the data and must not be confused with a synthetic ramp.
    "sindh": dict(mod="armored-fist", out="sindh-dem-90m.bin", zoom=13, target_m=90.0,
                  edge_blend_m=0.0, geodetic=True,
                  lon_min=66.55, lon_max=71.40, lat_min=24.65, lat_max=27.10),
}


def region_data_dir(mod_id):
    """The region names its own mod, so one invocation is enough whatever FB_MOD says."""
    d = os.path.join(mod.REPO_DIR, "mods", mod_id)
    with open(os.path.join(d, "mod.json"), encoding="utf-8") as f:
        return os.path.join(d, json.load(f)["data"])


def lonlat_to_tilef(lon, lat, z):
    n = 2 ** z
    x = (lon + 180.0) / 360.0 * n
    lat_rad = math.radians(lat)
    y = (1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n
    return x, y


def fetch_tile(base, z, x, y, retries=60, delay=0.25):
    """GET /t/terrain/z/x/y, retrying on 202 (fb-tiles is still fetching upstream) — same contract
    the native fb_stream_dem client already retries against."""
    url = f"{base}/t/terrain/{z}/{x}/{y}"
    for _ in range(retries):
        try:
            with urllib.request.urlopen(url, timeout=15) as r:
                if r.status == 200:
                    return x, y, r.read()
                r.read()
        except urllib.error.HTTPError as e:
            if e.code == 204:
                return x, y, None   # a real hole (open water/no coverage) -> Terrarium 0
            e.read()
        except Exception:
            pass
        time.sleep(delay)
    print(f"warn: tile {z}/{x}/{y} never landed, treating as a hole", file=sys.stderr)
    return x, y, None


def decode_terrarium(png_bytes):
    img = Image.open(BytesIO(png_bytes)).convert("RGB")
    a = np.asarray(img).astype(np.float64)
    return a[..., 0] * 256.0 + a[..., 1] + a[..., 2] / 256.0 - 32768.0


def smoothstep(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def sample_bilinear(grid, cols, rows, lon_min, lat_min, lon_max, lat_max, lon, lat):
    """The reader's own law (core/FBBakedDemElevation.cpp), reimplemented for --verify so the check
    measures the FILE and not this script's intermediate arrays."""
    cf = (lon - lon_min) / (lon_max - lon_min) * (cols - 1)
    rf = (lat - lat_min) / (lat_max - lat_min) * (rows - 1)
    c0 = min(max(int(cf), 0), cols - 2)
    r0 = min(max(int(rf), 0), rows - 2)
    tc, tr = cf - c0, rf - r0
    h00, h01 = grid[r0, c0], grid[r0, c0 + 1]
    h10, h11 = grid[r0 + 1, c0], grid[r0 + 1, c0 + 1]
    return (h00 + (h01 - h00) * tc) + ((h10 + (h11 - h10) * tc) - (h00 + (h01 - h00) * tc)) * tr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--region", required=True, choices=sorted(REGIONS))
    ap.add_argument("--base", default="http://localhost:8081")
    ap.add_argument("--out")
    ap.add_argument("--workers", type=int, default=16)
    ap.add_argument("--verify", type=int, default=0, help="re-ask /elev at N points and print the residual")
    args = ap.parse_args()

    r = REGIONS[args.region]
    lon_min, lon_max = r["lon_min"], r["lon_max"]
    lat_min, lat_max = r["lat_min"], r["lat_max"]
    zoom, target_m, edge_blend_m = r["zoom"], r["target_m"], r["edge_blend_m"]
    out = args.out or os.path.join(region_data_dir(r["mod"]), r["out"])

    x0f, y1f = lonlat_to_tilef(lon_min, lat_min, zoom)   # south-west corner -> larger y
    x1f, y0f = lonlat_to_tilef(lon_max, lat_max, zoom)   # north-east corner -> smaller y
    tx0, tx1 = int(math.floor(x0f)) - 1, int(math.floor(x1f)) + 1
    ty0, ty1 = int(math.floor(y0f)) - 1, int(math.floor(y1f)) + 1
    ntiles = (tx1 - tx0 + 1) * (ty1 - ty0 + 1)
    print(f"zoom {zoom}: tiles x[{tx0}..{tx1}] y[{ty0}..{ty1}] = {ntiles} tiles", file=sys.stderr)

    n = 2 ** zoom
    phi = math.radians(0.5 * (lat_min + lat_max))
    if r["geodetic"]:
        m_per_deg_lat = 111132.95 - 559.85 * math.cos(2 * phi) + 1.175 * math.cos(4 * phi)
        m_per_deg_lon = 111319.49 * math.cos(phi) / math.sqrt(1.0 - 0.00669438 * math.sin(phi) ** 2)
    else:
        m_per_deg_lat = 111320.0
        m_per_deg_lon = 111320.0 * math.cos(phi)
    cols = int(round((lon_max - lon_min) * m_per_deg_lon / target_m)) + 1
    rows = int(round((lat_max - lat_min) * m_per_deg_lat / target_m)) + 1
    print(f"output raster {cols}x{rows} ({cols*rows*2/1e6:.2f} MB raw)", file=sys.stderr)

    # Bilinear-sample the mosaic (mercator pixel space) at each output grid point (plain lat/lon space),
    # IN LATITUDE BANDS: a whole z13 mosaic of the mekong box is 21248x24320 px = 4.1 GB in float64 on
    # an 8.6 GB host, so the mosaic is never held whole. A band holds only the tile rows its own output
    # rows touch; the ~1 tile row of overlap between neighbouring bands is refetched from the LOCAL
    # fb-tiles, which is cheaper than any cache this script could keep.
    lons = lon_min + (lon_max - lon_min) * np.arange(cols) / (cols - 1)
    lats = lat_min + (lat_max - lat_min) * np.arange(rows) / (rows - 1)   # row 0 = south
    xf = (lons + 180.0) / 360.0 * n - tx0 * 1.0                           # mosaic-local, x is band-free
    px = np.clip(xf * 256.0, 0.0, (tx1 - tx0 + 1) * 256 - 1.001)
    x0i = px.astype(np.int64)
    fx = px - x0i
    yf_all = (1.0 - np.arcsinh(np.tan(np.radians(lats))) / math.pi) / 2.0 * n

    elev_i16 = np.empty((rows, cols), dtype="<i2")
    t0 = time.time()
    fetched = holes = 0
    for band0 in range(0, rows, BAND_ROWS):
        band1 = min(band0 + BAND_ROWS, rows)
        yf = yf_all[band0:band1]
        # row 0 = south = the LARGEST tile-y, so the band's tile range runs from its last row up.
        by0 = max(ty0, int(math.floor(yf.min())) - 1)
        by1 = min(ty1, int(math.floor(yf.max())) + 1)
        bh = (by1 - by0 + 1) * 256
        mosaic = np.zeros((bh, (tx1 - tx0 + 1) * 256), dtype=np.float64)
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            # No box in this table is near the antimeridian, so x in [tx0,tx1] c [0,n): no wrap needed.
            futs = [pool.submit(fetch_tile, args.base, zoom, x, y)
                    for y in range(by0, by1 + 1) for x in range(tx0, tx1 + 1)]
            for fut in as_completed(futs):
                x, y, raw = fut.result()
                fetched += 1
                if raw is None:
                    holes += 1
                    continue
                mosaic[(y - by0) * 256:(y - by0) * 256 + 256, (x - tx0) * 256:(x - tx0) * 256 + 256] = \
                    decode_terrarium(raw)

        py = np.clip((yf - by0) * 256.0, 0.0, bh - 1.001)
        y0i = py.astype(np.int64)
        fy = (py - y0i)[:, None]
        h00 = mosaic[np.ix_(y0i, x0i)]; h01 = mosaic[np.ix_(y0i, x0i + 1)]
        h10 = mosaic[np.ix_(y0i + 1, x0i)]; h11 = mosaic[np.ix_(y0i + 1, x0i + 1)]
        top = h00 + (h01 - h00) * fx
        bot = h10 + (h11 - h10) * fx
        elev = top + (bot - top) * fy

        if edge_blend_m > 0.0:
            dist_lon = np.minimum(lons - lon_min, lon_max - lons) * m_per_deg_lon
            dist_lat = np.minimum(lats[band0:band1] - lat_min, lat_max - lats[band0:band1]) * m_per_deg_lat
            elev *= smoothstep(np.minimum(dist_lon, dist_lat[:, None]) / edge_blend_m)

        elev_i16[band0:band1] = np.clip(np.round(elev), -32768, 32767).astype("<i2")
        print(f"  rows {band1}/{rows}, {fetched} tile fetches ({time.time()-t0:.1f}s)", file=sys.stderr)
    print(f"resampled {rows} rows from {fetched} tile fetches ({ntiles} unique) in "
          f"{time.time()-t0:.1f}s, {holes} holes", file=sys.stderr)

    with open(out, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<II", cols, rows))
        f.write(struct.pack("<dddd", lon_min, lat_min, lon_max, lat_max))
        f.write(struct.pack("<fI", 1.0, 0))
        f.write(elev_i16.tobytes())
    with open(out, "rb") as f:
        sha = hashlib.sha256(f.read()).hexdigest()
    print(f"wrote {out} ({os.path.getsize(out)/1e6:.2f} MB) sha256={sha}", file=sys.stderr)
    print(f"stats: min {elev_i16.min()} m, max {elev_i16.max()} m, mean {elev_i16.mean():.1f} m",
          file=sys.stderr)

    if args.verify:
        # Draw inside a 0.02 deg inset so no sample lands on the clamped border row/column.
        rng = random.Random(20260805)
        grid = elev_i16.astype(np.float64)
        errs = []
        for _ in range(args.verify):
            lat = rng.uniform(lat_min + 0.02, lat_max - 0.02)
            lon = rng.uniform(lon_min + 0.02, lon_max - 0.02)
            try:
                with urllib.request.urlopen(
                        f"{args.base}/elev?lat={lat:.6f}&lon={lon:.6f}&block=1", timeout=20) as resp:
                    if resp.status != 200:
                        continue
                    truth = float(resp.read())
            except Exception:
                continue
            errs.append(sample_bilinear(grid, cols, rows, lon_min, lat_min, lon_max, lat_max,
                                        lon, lat) - truth)
        if errs:
            e = np.array(errs)
            print(f"verify: n={len(e)} bias {e.mean():+.2f} m  rms {math.sqrt((e**2).mean()):.2f} m  "
                  f"|max| {np.abs(e).max():.2f} m", file=sys.stderr)


if __name__ == "__main__":
    main()
