#!/usr/bin/env python3
"""Track the honami's crest across a rendered sequence, in metres of WORLD and seconds.

  tools/wind_crest.py --dir seqA --frames 90 --dt 0.0166667 --yaw 70 --pitch -60

It never correlates images. Every pixel of every frame is unprojected through its own reversed-Z
depth into the camera's ENU frame (doc/render/renderer.md §1.9), binned onto a world lattice along
and across the declared wind, and reduced to the TOPMOST hit per column -- the canopy top as a
scalar field. The static roughness of that field is removed by subtracting each cell's own temporal
mean, which leaves the travelling part and nothing else; a two-dimensional FFT over (distance along
the wind, time) then reads the wavelength and the frequency off one peak, and the phase speed is
their product.

The camera does not move, so no registration is needed and no correlation coefficient is quoted:
the answer comes out in m/s.
"""

import argparse
import math
import os

import numpy as np

NEAR_M = 0.05


def rays(w, h, fov_deg, yaw_deg, pitch_deg):
    """Unit view rays in ENU (east, north, up) and the cosine of their off-axis angle."""
    f = 0.5 * h / math.tan(0.5 * fov_deg * math.pi / 180.0)
    y, p = yaw_deg * math.pi / 180.0, pitch_deg * math.pi / 180.0
    fwd = np.array([math.cos(p) * math.sin(y), math.cos(p) * math.cos(y), math.sin(p)])
    right = np.array([math.cos(y), -math.sin(y), 0.0])
    up = np.array([-math.sin(p) * math.sin(y), -math.sin(p) * math.cos(y), math.cos(p)])
    xs = np.arange(w) - 0.5 * (w - 1)
    ys = 0.5 * (h - 1) - np.arange(h)
    dx = np.repeat(xs[None, :], h, axis=0)
    dy = np.repeat(ys[:, None], w, axis=1)
    n = np.sqrt(dx * dx + dy * dy + f * f)
    d = (dx[..., None] * right + dy[..., None] * up + f * fwd) / n[..., None]
    return d, f / n


def canopy_top(depth, dirs, cosoff, wind, cell, half, halfe=None):
    """Topmost `up` per world column, on a lattice along/across the wind. NaN where nothing hit."""
    ok = depth > 1.0e-7
    rng = np.where(ok, NEAR_M / np.maximum(depth, 1.0e-7) / cosoff, 0.0)
    pe = rng * dirs[..., 0]
    pn = rng * dirs[..., 1]
    pu = rng * dirs[..., 2]
    xi = pe * wind[0] + pn * wind[1]
    eta = -pe * wind[1] + pn * wind[0]
    he = halfe if halfe else half
    i = np.floor(xi / cell).astype(np.int64)
    j = np.floor(eta / cell).astype(np.int64)
    keep = ok & (np.abs(i) < half) & (np.abs(j) < he)
    idx = ((i[keep] + half) * (2 * he) + (j[keep] + he)).astype(np.int64)
    out = np.full((2 * half) * (2 * he), -1.0e9)
    np.maximum.at(out, idx, pu[keep])
    return out.reshape(2 * half, 2 * he)


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--dir", required=True)
    a.add_argument("--frames", type=int, required=True)
    a.add_argument("--dt", type=float, required=True)
    a.add_argument("--yaw", type=float, required=True)
    a.add_argument("--pitch", type=float, required=True)
    a.add_argument("--fov", type=float, default=60.0)
    a.add_argument("--size", default="1280x720")
    a.add_argument("--wind-deg", type=float, default=250.0, help="the bearing it comes FROM")
    a.add_argument("--cell", type=float, default=0.03)
    a.add_argument("--half", type=int, default=560)
    a.add_argument("--half-eta", type=int, default=None)
    a.add_argument("--min-cover", type=float, default=0.9)
    a.add_argument("--lags", type=int, nargs="+", default=[2, 4, 6, 8])
    a.add_argument("--xrange", type=float, nargs=2, default=None,
                   help="restrict the band to these ground metres along the wind")
    args = a.parse_args()

    w, h = (int(v) for v in args.size.split("x"))
    ang = args.wind_deg * math.pi / 180.0
    wind = (-math.sin(ang), -math.cos(ang))
    dirs, cosoff = rays(w, h, args.fov, args.yaw, args.pitch)

    stack = []
    for k in range(args.frames):
        d = np.fromfile(os.path.join(args.dir, "%04d.f32" % k), dtype=np.float32).reshape(h, w)
        stack.append(canopy_top(d.astype(np.float64), dirs, cosoff, wind, args.cell, args.half,
                                args.half_eta))
    z = np.stack(stack)                                   # (t, xi, eta)

    live = (z > -1.0e8)
    # A CELL counts when it is covered in nearly every frame; a COLUMN counts when enough of its
    # cells do. Demanding every frame of every cell shrank the band to two handfuls of bins.
    solid = live.mean(axis=0) >= args.min_cover
    cols = solid.sum(axis=1)
    if args.xrange:
        x0, x1 = args.xrange
        idx = (np.arange(len(cols)) - args.half) * args.cell
        inr = (idx >= x0) & (idx <= x1)
        keep = inr & (cols >= max(1.0, 0.25 * cols[inr].max()))
    else:
        keep = cols >= 0.25 * cols.max()
    lo, hi = int(np.argmax(keep)), len(keep) - 1 - int(np.argmax(keep[::-1]))
    z = np.where(live, z, np.nan)[:, lo:hi + 1, :]
    band = solid[lo:hi + 1, :]
    print("lattice %.0f mm, columns along the wind %d..%d (%.2f m), %d cells in the band"
          % (args.cell * 1000.0, lo - args.half, hi - args.half,
             (hi - lo + 1) * args.cell, int(band.sum())))
    print("canopy-top sd over the whole field, frame 0: %.4f m" % np.nanstd(np.where(band, z[0], np.nan)))

    zb = np.where(band[None, :, :], z, np.nan)
    zb = zb - np.nanmean(zb, axis=0, keepdims=True)
    prof = np.nanmean(zb, axis=2)
    prof = np.nan_to_num(prof)
    prof -= prof.mean(axis=0, keepdims=True)
    print("travelling part of the canopy top, rms over (x, t): %.4f m" % prof.std())

    def spectrum(pr, dt, cell):
        n_t, n_x = pr.shape
        wn = np.hanning(n_t)[:, None] * np.hanning(n_x)[None, :]
        s2 = np.abs(np.fft.rfft2(pr * wn))
        s2[0, :] = 0.0
        it, ik = np.unravel_index(np.argmax(s2), s2.shape)

        def pb(y0, y1, y2):
            dd = y0 - 2.0 * y1 + y2
            return 0.5 * (y0 - y2) / dd if abs(dd) > 1e-30 else 0.0

        fit = it + pb(s2[(it - 1) % n_t, ik], s2[it, ik], s2[(it + 1) % n_t, ik])
        fik = ik + (pb(s2[it, ik - 1], s2[it, ik], s2[it, ik + 1]) if 0 < ik < s2.shape[1] - 1 else 0.0)
        fr = (fit - n_t) / (n_t * dt) if fit > n_t / 2 else fit / (n_t * dt)
        lm = (n_x * cell) / fik if fik else float("inf")
        return abs(fr), lm, -fr * lm

    # THE DECISIVE WORLD-FIXEDNESS TEST, and it costs nothing: the band spans 0..12 m of ground, so a
    # wave that stood on the SCREEN would have a constant wavelength in PIXELS and therefore a
    # wavelength in metres that grows with distance. Measured in thirds of the band it does not.
    nb = prof.shape[1] // 3
    for k, name in ((0, "near"), (1, "middle"), (2, "far")):
        f3, l3, c3 = spectrum(prof[:, k * nb:(k + 1) * nb], args.dt, args.cell)
        print("  %-6s third (%.2f..%.2f m of ground): f = %.3f Hz, lambda = %.4f m, c = %+.4f m/s"
              % (name, (lo + k * nb - args.half) * args.cell,
                 (lo + (k + 1) * nb - args.half) * args.cell, f3, l3, c3))

    nt, nx = prof.shape
    win = np.hanning(nt)[:, None] * np.hanning(nx)[None, :]
    sp = np.abs(np.fft.rfft2(prof * win))
    sp[0, :] = 0.0
    ti, xk = np.unravel_index(np.argmax(sp), sp.shape)

    def parab(y0, y1, y2):
        d = y0 - 2.0 * y1 + y2
        return 0.5 * (y0 - y2) / d if abs(d) > 1e-30 else 0.0

    fi = ti + parab(sp[(ti - 1) % nt, xk], sp[ti, xk], sp[(ti + 1) % nt, xk])
    fx = xk + (parab(sp[ti, xk - 1], sp[ti, xk], sp[ti, xk + 1]) if 0 < xk < sp.shape[1] - 1 else 0.0)
    # numpy's forward transform carries exp(-2*pi*i*(m*t/T + n*x/X)), so a wave running in +x with
    # a positive frequency lands at a POSITIVE wavenumber row and a WRAPPED time row.
    freq = (fi - nt) / (nt * args.dt) if fi > nt / 2 else fi / (nt * args.dt)
    lam = (nx * args.cell) / fx if fx else float("inf")
    print("peak of the (x, t) spectrum: |f| = %.3f Hz, lambda = %.4f m, c = %+.4f m/s"
          % (abs(freq), lam, -freq * lam))

    # Independent of the spectrum: the shift in METRES that best lines frame t up with frame t+lag.
    for lag in args.lags:
        sh = []
        for t in range(nt - lag):
            a0 = prof[t] - prof[t].mean()
            a1 = prof[t + lag] - prof[t + lag].mean()
            c = np.correlate(a1, a0, mode="full")
            m = int(np.argmax(c))
            if 0 < m < len(c) - 1:
                m = m + parab(c[m - 1], c[m], c[m + 1])
            sh.append((m - (nx - 1)) * args.cell)
        sh = np.array(sh)
        print("lag %2d (%.4f s): crest shift median %+.4f m  ->  c = %+.4f m/s   (sd of c %.4f)"
              % (lag, lag * args.dt, np.median(sh), np.median(sh) / (lag * args.dt),
                 sh.std() / (lag * args.dt)))


if __name__ == "__main__":
    main()
