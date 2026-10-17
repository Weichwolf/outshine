"""Shared source-index Hermite derivatives and physical endpoint radius choices."""

import numpy as np
from outshine_ref.vegetation.spine_frames import unit


def path_data(positions, radii, path):
    ids = np.asarray(path)
    if ids.ndim != 1 or not 2 <= len(ids) <= 1025:
        raise ValueError("bounded source-index path required")
    points = positions[ids]
    chord = np.diff(points, axis=0)
    lengths = np.linalg.norm(chord, axis=1)
    tangent = unit(chord)
    derivatives = np.empty_like(points)
    derivatives[0] = tangent[0] * lengths[0]
    derivatives[-1] = tangent[-1] * lengths[-1]
    if len(ids) > 2:
        derivatives[1:-1] = (
            unit(tangent[:-1] + tangent[1:])
            * np.minimum(lengths[:-1], lengths[1:])[:, None]
        )
    radius = radii[ids].copy()
    if ids[0] != 0:
        radius[0] = radius[1]
    radius[-1] *= 0.35
    return points, derivatives, radius
