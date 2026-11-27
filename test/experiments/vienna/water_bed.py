"""Optical depth of an inferred bed beside mapped natural shorelines."""

from dataclasses import dataclass

import numpy as np
import shapely
from shore_grade import coast_feet, smoothstep

BED_REACH_M = 10.0
BED_TEXEL_M = 0.75
EXTINCTION_PER_M = np.array([0.95, 0.60, 0.44])
BED_ALBEDO = (0.119, 0.102, 0.067)


@dataclass(frozen=True)
class WaterBed:
    origin: tuple[float, float]
    transmission: np.ndarray

    @property
    def transform(self):
        height, width = self.transmission.shape[:2]
        return (*self.origin, 1 / (width * BED_TEXEL_M), 1 / (height * BED_TEXEL_M))


def nearshore_bed(shore, extent, *, source=None):
    if not shore.contacts:
        return None
    coast = shapely.union_all(shore.contacts)
    strip = coast.buffer(BED_REACH_M, quad_segs=3).intersection(extent)
    if strip.is_empty:
        return None
    lo, hi = np.asarray(strip.bounds).reshape(2, 2)
    first = np.floor(lo / BED_TEXEL_M).astype(int) - 1
    stop = np.ceil(hi / BED_TEXEL_M).astype(int) + 1
    x, y = [(np.arange(a, b) + 0.5) * BED_TEXEL_M for a, b in zip(first, stop)]
    xx, yy = np.meshgrid(x, y)
    mask = shapely.contains_xy(strip, xx, yy)
    points = shapely.points(xx[mask], yy[mask])
    index, owners = shore.coast_segments
    (rows, segments), distance = index.query_nearest(
        points, max_distance=BED_REACH_M, all_matches=False, return_distance=True
    )
    if not len(rows):
        return None
    portions = index.geometries[segments]
    foot = coast_feet(portions, points[rows])
    coast_xy = shapely.get_coordinates(foot)
    natural = shore.protection.weight(foot, coast_xy, np.full(len(rows), 2.5))
    body_ids = owners[segments]
    if source is not None:
        for i, body in enumerate(shore.bodies):
            if body.control is not None and body.control.retaining_limits_m is not None:
                uses = body_ids == i
                wall, _ = body.control.retaining_profile(source, coast_xy[uses])
                natural[uses] *= 1 - wall
    # The bed slopes and mean two-way optical path are reference parameters.
    slopes = [
        0.5
        if body.control is not None
        else (0.42 if body.water_class == "river" else 0.30)
        for body in shore.bodies
    ]
    depth = distance * np.asarray(slopes)[body_ids]
    transmission = np.exp(-2.5 * depth[:, None] * EXTINCTION_PER_M)
    off_bank = 1 - smoothstep(np.clip((distance - 7.0) / 3.0, 0, 1))
    # Fade the local product to deep-water shading before the regional seam.
    edge = shapely.distance(points[rows], extent.boundary)
    patch_weight = smoothstep(np.clip(edge / BED_REACH_M, 0, 1))
    values = np.zeros((len(y), len(x), 3), dtype="f4")
    values.reshape(-1, 3)[np.flatnonzero(mask)[rows]] = (
        transmission * (natural * off_bank * patch_weight)[:, None]
    )
    return WaterBed(tuple(first * BED_TEXEL_M), values)
