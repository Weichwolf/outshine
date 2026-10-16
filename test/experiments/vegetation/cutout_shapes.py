"""Own deterministic asymmetric contour recipes; no species/asset reconstruction."""

import numpy as np
import shapely
from scipy.interpolate import PchipInterpolator


def contour(blade, seed=7, kind="lobed"):
    if kind not in ("entire", "lobed", "toothed"):
        raise ValueError("declared authored contour family required")
    rng = np.random.default_rng(seed)
    t = np.linspace(0, 1, 97)
    sides = []
    for _ in range(2):
        controls = rng.uniform(0.83, 0.99, 9)
        controls[0] = 1
        width = PchipInterpolator(np.linspace(0, 1, 9), controls)(t)
        if kind == "lobed":
            centres = np.array((0.17, 0.36, 0.56, 0.77)) + rng.uniform(-0.035, 0.035, 4)
            for centre in centres:
                width -= rng.uniform(0.25, 0.42) * np.exp(
                    -(((t - centre) / rng.uniform(0.025, 0.045)) ** 2)
                )
        elif kind == "toothed":
            stations = np.r_[0, np.sort(rng.uniform(0.04, 0.94, 25)), 1]
            depths = rng.uniform(0.82, 0.99, len(stations))
            depths[0] = 1
            width *= np.interp(t, stations, depths)
        width[0] = 1
        # Avoid a finite parameter cap at the collapsed carrier tip.
        width[-1] = 0
        sides.append(np.c_[width, t])
    bottom = -blade.petiole / blade.length
    points = [
        (-1, bottom),
        (1, bottom),
        *sides[0],
        *[(-s, t) for s, t in sides[1][-2::-1]],
    ]
    return shapely.Polygon(points)
