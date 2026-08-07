#!/usr/bin/env python3
"""Is the WAVE over the ground or over the screen? Four frames, one number, in millimetres.

  tools/wind_fixed.py --a0 a0.f32 --a1 a1.f32 --b0 b0.f32 --b1 b1.f32 --step 0.5 --yaw 70 --pitch -60

A0/A1 are one standpoint at two wind clocks, B0/B1 the standpoint moved by `--step` east at the same
two clocks. The canopy top carries 0.26 m of STATIC roughness against 9 mm of wave, so registering
the canopy top itself would pass whatever the wave does. Differencing the two clocks at each
standpoint removes the static field exactly and leaves the wave alone; the two difference fields are
then registered against each other in the CAMERA's own plane (doc/render/renderer.md §1.9).

World-fixed predicts the camera step. Screen-fixed predicts zero. The two are 0.5 m apart, and the
answer is the argmin.
"""

import argparse
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wind_crest

NEAR_M = 0.05


def field(path, dirs, cosoff, w, h, cell, half):
    """Topmost `up` per (right, forward) cell of the CAMERA's own plane."""
    d = np.fromfile(path, dtype=np.float32).reshape(h, w).astype(np.float64)
    ok = d > 1.0e-7
    rng = np.where(ok, NEAR_M / np.maximum(d, 1.0e-7) / cosoff, 0.0)
    p = rng[..., None] * dirs
    i = np.floor(p[..., 0] / cell).astype(np.int64)
    j = np.floor(p[..., 1] / cell).astype(np.int64)
    keep = ok & (np.abs(i) < half) & (np.abs(j) < half)
    idx = ((i[keep] + half) * (2 * half) + (j[keep] + half)).astype(np.int64)
    out = np.full((2 * half) * (2 * half), -1.0e9)
    np.maximum.at(out, idx, p[..., 2][keep])
    return out.reshape(2 * half, 2 * half)


def main():
    a = argparse.ArgumentParser()
    for k in ("a0", "a1", "b0", "b1"):
        a.add_argument("--" + k, required=True)
    a.add_argument("--step", type=float, required=True, help="metres EAST from A to B")
    a.add_argument("--yaw", type=float, required=True)
    a.add_argument("--pitch", type=float, required=True)
    a.add_argument("--fov", type=float, default=60.0)
    a.add_argument("--size", default="1280x720")
    a.add_argument("--cell", type=float, default=0.03)
    a.add_argument("--half", type=int, default=560)
    a.add_argument("--search", type=int, default=30)
    a.add_argument("--smooth", type=int, default=1)
    args = a.parse_args()

    w, h = (int(v) for v in args.size.split("x"))
    # The measurement lives in the CAMERA's plane, so the rays are built with yaw 0 and the camera
    # step is projected onto (right, forward) by hand below.
    dirs, cosoff = wind_crest.rays(w, h, args.fov, 0.0, args.pitch)

    f = {k: field(getattr(args, k), dirs, cosoff, w, h, args.cell, args.half)
         for k in ("a0", "a1", "b0", "b1")}
    live = {k: v > -1.0e8 for k, v in f.items()}
    da = np.where(live["a0"] & live["a1"], f["a1"] - f["a0"], np.nan)
    db = np.where(live["b0"] & live["b1"], f["b1"] - f["b0"], np.nan)
    if args.smooth > 1:
        # A blade tip is thinner than a bin, so the topmost hit is a noisy estimator of the canopy
        # top. The wave is 0.44 m long; a box a fifth of that keeps it and takes the sampling noise
        # out, which is the difference between an argmin and an argmin one can believe.
        k = args.smooth
        for m in (da, db):
            m[~np.isfinite(m)] = np.nan
        def box(m):
            c = np.nan_to_num(m)
            n = np.isfinite(m).astype(np.float64)
            for ax in (0, 1):
                c = np.apply_along_axis(lambda v: np.convolve(v, np.ones(k), "same"), ax, c)
                n = np.apply_along_axis(lambda v: np.convolve(v, np.ones(k), "same"), ax, n)
            return np.where(n >= 0.5 * k * k, c / np.maximum(n, 1e-9), np.nan)
        da, db = box(da), box(db)
    print("the wave alone, |dz| median over the field: A %.4f m, B %.4f m  (box %d bins)"
          % (np.nanmedian(np.abs(da)), np.nanmedian(np.abs(db)), args.smooth))

    y = args.yaw * math.pi / 180.0
    pr = -args.step * math.cos(y)                    # world moves opposite to the camera
    pf = -args.step * math.sin(y)
    print("predicted shift of B against A in the camera plane: (dr, df) = (%+.4f, %+.4f) m"
          % (pr, pf))

    best, null = None, None
    for di in range(-args.search, args.search + 1):
        for dj in range(-args.search, args.search + 1):
            sa = da[args.search:-args.search, args.search:-args.search]
            sb = db[args.search + di:db.shape[0] - args.search + di,
                    args.search + dj:db.shape[1] - args.search + dj]
            m = np.nanmedian(np.abs(sb - sa))
            if not np.isfinite(m):
                continue
            if di == 0 and dj == 0:
                null = m
            if best is None or m < best[0]:
                best = (m, di, dj)
    print("best fit (dr, df) = (%+.4f, %+.4f) m, median |dz| = %.4f m"
          % (best[1] * args.cell, best[2] * args.cell, best[0]))
    print("the same at zero shift (the screen-fixed hypothesis): median |dz| = %.4f m" % null)


if __name__ == "__main__":
    main()
