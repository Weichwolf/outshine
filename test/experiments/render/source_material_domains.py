"""Local material domains before optional world-space surface construction."""

from typing import NamedTuple

import numpy as np
import shapely
from column_roof_patches import bottom_m, roof_patches
from source_surface_patches import wall_domains


class MaterialDomain(NamedTuple):
    domain: object
    local_origin: np.ndarray
    local_axes: np.ndarray
    side: int
    gable: bool
    orientation: int = 1


def domain_axes(domain, frame):
    if domain.side == 1:
        return frame[:2] + domain.local_axes[:, 2, None] * frame[2]
    return np.stack((domain.local_axes[0, :2] @ frame[:2], frame[2]))


def shape_material_domains(surface, shape):
    """Keep original side ordinals, deck cuts and supported roof/wall domains."""
    roofs = roof_patches(shape)
    patches = [
        MaterialDomain(
            domain,
            np.array([0, 0, plane[2]]),
            np.array([[1, 0, plane[0]], [0, 1, plane[1]]]),
            1,
            False,
        )
        for domain, plane in roofs
    ]
    sides = {}
    for ring in [shape["ring"], *shape["holes"]]:
        for a in ring:
            key = tuple(a)
            if key in sides:
                raise ValueError("ambiguous source-side origin")
            sides[key] = 2 + len(sides)
    deck = shape["seat_m"] + shape["foot_m"] + shape["eaves_m"]
    if shape["roof_kind"] == 0:
        deck += shape["rise_m"]
    for domain, a, along, orientation in wall_domains(
        shape, roofs, bottom_m(surface, shape)
    ):
        x0, z0, x1, z1 = domain.bounds
        intervals = [(False, z0, min(z1, deck)), (True, max(z0, deck), z1)]
        for gable, low, high in intervals:
            if high <= low:
                continue
            piece = shapely.intersection(domain, shapely.box(x0, low, x1, high))
            if piece.area > 0:
                patches.append(
                    MaterialDomain(
                        piece,
                        np.r_[a, 0.0],
                        np.array([[*along, 0], [0, 0, 1]]),
                        sides[tuple(a)],
                        gable,
                        orientation,
                    )
                )
    return patches
