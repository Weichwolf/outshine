#!/usr/bin/env python3
"""FlightBox — bake_swiss_dem.py: bakes a Switzerland-shaped DEM island into
sim/assets/swiss-dem-90m.bin for FBBakedDemElevation (the gym's --elev swiss).

Manual, one-off tool (NOT a build dependency): run it against a LOCAL fb-tiles
(default http://localhost:8081) once; the output asset is checked in.

    python3 tools/bake_swiss_dem.py [--base URL] [--out PATH]

Why the tile route, not point /elev requests: the target raster is ~3836x2462
points (~9.4M) — at one HTTP round trip per point that is hours even
parallelized. fb-tiles' public /t/terrain/z/x/y route instead serves the
raw Terrarium-encoded DEM PNGs (elev = R*256+G+B/256-32768, the same
mapping tiles/elev.c and world/FBTerrainField.cpp already decode) that
back /elev itself; at zoom 11 (~52-53 m/px over this bbox, finer than the
90 m output raster) covering the bbox + a 1-tile margin takes ~580 tile
fetches, each cached by fb-tiles' own upstream fetch/cache, then one
bilinear resample onto the 90 m grid in-process. Verified byte-for-byte
against /elev at multiple points (see the task report) before baking.

Output format (little-endian, magic "FBDEM01"): see FBBakedDemElevation.cpp's
banner for the exact byte layout this script writes.
"""
import argparse
import math
import struct
import sys
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from io import BytesIO

import numpy as np
from PIL import Image

# Insel-Bounding-Box (task spec): 5.96-10.49 degE / 45.82-47.81 degN.
LON_MIN, LON_MAX, LAT_MIN, LAT_MAX = 5.96, 10.49, 45.82, 47.81
TARGET_M = 90.0          # output raster spacing at the bbox's mid-latitude
DEM_ZOOM = 11             # Terrarium source zoom (~52-53 m/px here, finer than 90 m)
EDGE_BLEND_M = 15000.0    # smoothstep the outer ~15 km of the box down to 0 m (island contract)
MAGIC = b"FBDEM01\0"


def lonlat_to_tilef(lon, lat, z):
    n = 2 ** z
    x = (lon + 180.0) / 360.0 * n
    lat_rad = math.radians(lat)
    y = (1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n
    return x, y


def fetch_tile(base, z, x, y, retries=40, delay=0.25):
    """GET /t/terrain/z/x/y, retrying on 202 (fb-tiles is still fetching upstream) — same contract
    the native fb_stream_dem client already retries against."""
    url = f"{base}/t/terrain/{z}/{x}/{y}"
    for attempt in range(retries):
        try:
            with urllib.request.urlopen(url, timeout=10) as r:
                if r.status == 200:
                    return x, y, r.read()
                body = r.read()
        except urllib.error.HTTPError as e:
            if e.code == 204:
                return x, y, None   # a real hole (open water/no coverage) -> Terrarium 0
            body = e.read()
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="http://localhost:8081")
    ap.add_argument("--out", default="assets/swiss-dem-90m.bin")
    ap.add_argument("--workers", type=int, default=16)
    args = ap.parse_args()

    x0f, y1f = lonlat_to_tilef(LON_MIN, LAT_MIN, DEM_ZOOM)   # south-west corner -> larger y
    x1f, y0f = lonlat_to_tilef(LON_MAX, LAT_MAX, DEM_ZOOM)   # north-east corner -> smaller y
    tx0, tx1 = int(math.floor(x0f)) - 1, int(math.floor(x1f)) + 1
    ty0, ty1 = int(math.floor(y0f)) - 1, int(math.floor(y1f)) + 1
    ntiles = (tx1 - tx0 + 1) * (ty1 - ty0 + 1)
    print(f"zoom {DEM_ZOOM}: tiles x[{tx0}..{tx1}] y[{ty0}..{ty1}] = {ntiles} tiles", file=sys.stderr)

    n = 2 ** DEM_ZOOM
    mosaic_w, mosaic_h = (tx1 - tx0 + 1) * 256, (ty1 - ty0 + 1) * 256
    mosaic = np.zeros((mosaic_h, mosaic_w), dtype=np.float64)

    t0 = time.time()
    done = 0
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        # bbox is nowhere near the antimeridian at this zoom (x in [tx0,tx1] c [0,n)), so no wrap needed.
        futs = [pool.submit(fetch_tile, args.base, DEM_ZOOM, x, y) for y in range(ty0, ty1 + 1) for x in range(tx0, tx1 + 1)]
        for fut in as_completed(futs):
            x, y, raw = fut.result()
            done += 1
            if done % 100 == 0 or done == ntiles:
                print(f"  {done}/{ntiles} tiles ({time.time()-t0:.1f}s)", file=sys.stderr)
            if raw is None:
                continue
            elev = decode_terrarium(raw)
            ox, oy = (x - tx0) * 256, (y - ty0) * 256
            mosaic[oy:oy + 256, ox:ox + 256] = elev
    print(f"fetched {ntiles} tiles in {time.time()-t0:.1f}s", file=sys.stderr)

    # Output raster dimensions: ~90 m spacing at the bbox mid-latitude (task spec ~3900x2450).
    lat_mid = 0.5 * (LAT_MIN + LAT_MAX)
    m_per_deg_lat = 111320.0
    m_per_deg_lon = 111320.0 * math.cos(math.radians(lat_mid))
    cols = int(round((LON_MAX - LON_MIN) * m_per_deg_lon / TARGET_M)) + 1
    rows = int(round((LAT_MAX - LAT_MIN) * m_per_deg_lat / TARGET_M)) + 1
    print(f"output raster {cols}x{rows} ({cols*rows*2/1e6:.2f} MB raw)", file=sys.stderr)

    # Bilinear-sample the mosaic (mercator pixel space) at each output grid point (plain lat/lon space).
    lons = LON_MIN + (LON_MAX - LON_MIN) * np.arange(cols) / (cols - 1)
    lats = LAT_MIN + (LAT_MAX - LAT_MIN) * np.arange(rows) / (rows - 1)   # row 0 = south
    lon_grid, lat_grid = np.meshgrid(lons, lats)   # (rows, cols)

    xf = (lon_grid + 180.0) / 360.0 * n - tx0 * 1.0   # mosaic-local tile-fraction x
    lat_rad = np.radians(lat_grid)
    yf = (1.0 - np.arcsinh(np.tan(lat_rad)) / math.pi) / 2.0 * n - ty0 * 1.0
    px = xf * 256.0
    py = yf * 256.0
    px = np.clip(px, 0.0, mosaic_w - 1.001)
    py = np.clip(py, 0.0, mosaic_h - 1.001)
    x0i = px.astype(np.int64); y0i = py.astype(np.int64)
    fx = px - x0i; fy = py - y0i
    h00 = mosaic[y0i, x0i]; h01 = mosaic[y0i, x0i + 1]
    h10 = mosaic[y0i + 1, x0i]; h11 = mosaic[y0i + 1, x0i + 1]
    top = h00 + (h01 - h00) * fx
    bot = h10 + (h11 - h10) * fx
    elev = top + (bot - top) * fy

    # Insel edge blend: the outer EDGE_BLEND_M of the box smoothsteps down to 0 (avoids a hard cliff
    # at the bbox border where real terrain is non-zero, e.g. the Alps near the southern edge).
    dist_w = (lon_grid - LON_MIN) * m_per_deg_lon
    dist_e = (LON_MAX - lon_grid) * m_per_deg_lon
    dist_s = (lat_grid - LAT_MIN) * m_per_deg_lat
    dist_n = (LAT_MAX - lat_grid) * m_per_deg_lat
    dist_edge = np.minimum(np.minimum(dist_w, dist_e), np.minimum(dist_s, dist_n))
    weight = smoothstep(dist_edge / EDGE_BLEND_M)
    elev *= weight

    elev_i16 = np.clip(np.round(elev), -32768, 32767).astype("<i2")

    with open(args.out, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<II", cols, rows))
        f.write(struct.pack("<dddd", LON_MIN, LAT_MIN, LON_MAX, LAT_MAX))
        f.write(struct.pack("<fI", 1.0, 0))
        f.write(elev_i16.tobytes())
    import os
    print(f"wrote {args.out} ({os.path.getsize(args.out)/1e6:.2f} MB)", file=sys.stderr)


if __name__ == "__main__":
    main()
