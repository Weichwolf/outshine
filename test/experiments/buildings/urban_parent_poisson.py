"""Spatial marked-Poisson box experiment, not the categorical lattice model.

Conditioned void probability is exp(-integral over light footprint minus view
footprint). Coarse source-density bins bound work independently of recipe cells.
GEOS is an offline Python geometric reference, not a native/frame-path proposal.
"""

import numpy as np
import shapely
from shapely.strtree import STRtree
from urban_parent_volume import centres_at


def calibrated_rates(coverage, probabilities, side):
    """Match the complete vertical height CDF in a homogeneous interior."""
    tail = np.cumsum(probabilities[::-1])[::-1]
    void = 1 - np.asarray(coverage)[..., None] * tail
    above = np.concatenate((void[..., 1:], np.ones_like(void[..., :1])), axis=-1)
    return (np.log(above) - np.log(void)) / side**2


def swept_squares(first, last, half):
    """Convex Minkowski sum of an XY segment with an axis-aligned square."""
    offsets = half * np.array([[-1, -1], [1, -1], [1, 1], [-1, 1]])
    corners = np.concatenate(
        (first[:, None] + offsets, last[:, None] + offsets), axis=1
    )
    return shapely.convex_hull(shapely.multipoints(corners))


def view_footprints(origins, points, height, half):
    """Closed camera prefix inside z=[0,height]; empty above a roof endpoint."""
    active = points[:, 2] < height - 1e-8
    delta = points - origins
    start = np.divide(
        height - origins[:, 2],
        delta[:, 2],
        out=np.zeros(len(points)),
        where=delta[:, 2] != 0,
    )
    start = np.where(origins[:, 2] > height, np.clip(start, 0, 1), 0)
    first = origins + start[:, None] * delta
    shapes = swept_squares(first[:, :2], points[:, :2], half)
    return np.where(active, shapes, shapely.Polygon())


class PoissonParent:
    """Derived coarse density/geometry only; no recipe cells or resolved input."""

    def __init__(self, parent):
        self.parent = parent
        self.side = parent.frame[2] * parent.frame[4] * parent.frame[5]
        centres = parent.centre_fraction()
        height, width = centres.shape
        if height != width:
            raise ValueError("square coarse density grid required")
        extent = parent.frame[2] * parent.frame[3]
        self.pitch = extent / width
        x, y = np.meshgrid(np.arange(width), np.arange(height))
        self.regions = shapely.box(
            x.ravel() * self.pitch,
            y.ravel() * self.pitch,
            (x.ravel() + 1) * self.pitch,
            (y.ravel() + 1) * self.pitch,
        )
        coverage = centres.ravel() * (1 - parent.frame[6]) * parent.frame[5] ** 2
        self.rates = calibrated_rates(coverage, parent.probabilities, self.side)
        self.index = STRtree(self.regions)
        self.work = {
            "coarse_density_bins": len(self.regions),
            "density_rate_array_bytes": self.rates.nbytes,
            "receiver_samples": 0,
            "swept_polygons": 0,
            "view_subtractions": 0,
            "coarse_overlap_pairs": 0,
        }

    def integral(self, shapes, level):
        result = np.zeros(len(shapes))
        ray, region = self.index.query(shapes, predicate="intersects")
        areas = shapely.area(shapely.intersection(shapes[ray], self.regions[region]))
        np.add.at(result, ray, areas * self.rates[region, level])
        self.work["coarse_overlap_pairs"] += len(ray)
        return result

    def conditional_visibility(self, origins, points, light, *, joint=True):
        """Exact void ratio for this Poisson model, not a lattice error bound."""
        origins, points, light = np.broadcast_arrays(origins, points, light)
        if (
            origins.ndim != 2
            or origins.shape[1] != 3
            or not np.isfinite(origins).all()
            or not np.isfinite(points).all()
            or not np.isfinite(light).all()
            or np.any(light[:, 2] <= 0)
            or np.any(points[:, 2] < 0)
            or np.any(origins[:, 2] < 0)
        ):
            raise ValueError("finite above-base geometry and upward light required")
        depth = np.zeros(len(points))
        self.work["receiver_samples"] += len(points)
        for level, height in enumerate(self.parent.heights):
            active = np.flatnonzero(points[:, 2] < height - 1e-8)
            p, at = points[active], light[active]
            end = p + ((height - p[:, 2]) / at[:, 2])[:, None] * at
            footprint = swept_squares(p[:, :2], end[:, :2], self.side / 2)
            self.work["swept_polygons"] += len(active)
            if joint:
                view = view_footprints(origins[active], p, height, self.side / 2)
                footprint = shapely.difference(footprint, view)
                self.work["swept_polygons"] += len(active)
                self.work["view_subtractions"] += len(active)
            depth[active] += self.integral(footprint, level)
        return np.exp(-depth)

    def extinction(self, parent, points, mode):
        """Local slab approximation for primary walls, separate from exact void."""
        coverage = (
            centres_at(parent, points, mode)
            * (1 - parent.frame[6])
            * parent.frame[5] ** 2
        )
        rates = calibrated_rates(coverage, parent.probabilities, self.side)
        return self.side * (rates * (parent.heights[None] >= points[:, 2, None])).sum(
            axis=1
        )
