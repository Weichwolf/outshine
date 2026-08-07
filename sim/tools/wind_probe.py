#!/usr/bin/env python3
"""The STATE channel of the wind wave: the same (x, t) spectrum as wind_crest.py, off numbers.

  tools/wind_probe.py probe.csv

Input is `gpu_walk --wind-probe`, which samples the shipped WindField -- the object the ground-cover
stage is handed -- on a world line along the declared wind. Nothing here is rendered, so a
disagreement with the image measurement is a rendering defect and an agreement is not a tautology:
the two paths share no code below the constants.
"""

import sys

import numpy as np


def main():
    path = sys.argv[1]
    with open(path) as f:
        head = f.readline()
    meta = dict(kv.split("=") for kv in head.lstrip("# ").split())
    dx, dt = float(meta["dxM"]), float(meta["dtS"])
    a = np.loadtxt(path, delimiter=",", comments="#")
    a = a - a.mean(axis=0, keepdims=True)
    nt, nx = a.shape

    win = np.hanning(nt)[:, None] * np.hanning(nx)[None, :]
    sp = np.abs(np.fft.rfft2(a * win))
    sp[0, :] = 0.0
    ti, xk = np.unravel_index(np.argmax(sp), sp.shape)

    def parab(y0, y1, y2):
        d = y0 - 2.0 * y1 + y2
        return 0.5 * (y0 - y2) / d if abs(d) > 1e-30 else 0.0

    fi = ti + parab(sp[(ti - 1) % nt, xk], sp[ti, xk], sp[(ti + 1) % nt, xk])
    fx = xk + (parab(sp[ti, xk - 1], sp[ti, xk], sp[ti, xk + 1]) if 0 < xk < sp.shape[1] - 1 else 0.0)
    freq = (fi - nt) / (nt * dt) if fi > nt / 2 else fi / (nt * dt)
    lam = (nx * dx) / fx

    print("declared by the field: c = %s m/s, f = %s Hz, lambda = %s m"
          % (meta["phaseSpeedMs"], meta["eigenHz"], meta["waveLenM"]))
    print("recovered from the sampled tip angle: |f| = %.4f Hz, lambda = %.4f m, c = %+.4f m/s"
          % (abs(freq), lam, -freq * lam))
    print("tip angle over the line: mean %.3f deg, min %.3f, max %.3f, rms of the travelling part %.4f"
          % (np.loadtxt(path, delimiter=",", comments="#").mean(),
             np.loadtxt(path, delimiter=",", comments="#").min(),
             np.loadtxt(path, delimiter=",", comments="#").max(), a.std()))


if __name__ == "__main__":
    main()
