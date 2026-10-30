"""Construct extra micrometre room before whole Float32 body admission."""

import numpy as np
from outshine_ref.contracts import BuildingFootprint, HeightInterval
from rail_building_grid import BAND_M
from rail_occupancy_reference import RailReference
from stair_occupancy_candidates import CandidateQuery
from stair_occupancy_geometry import OccupancyError, polygonal_conflict


def round_bounds(products):
    errors = [
        body.vertices.astype("f4").astype(float) - body.vertices
        for body in [products.deck, *products.supports]
    ]
    xy = max(float(np.linalg.norm(e[:, :2], axis=1).max()) for e in errors)
    z = max(float(abs(e[:, 2]).max()) for e in errors)
    return xy, z


def margins(footprint, errors):
    extent = max(abs(v) for v in footprint.geometry.bounds)
    height = max(abs(footprint.heights.lower_m), abs(footprint.heights.upper_m))
    # Half an ULP bounds rounding anywhere within each original coordinate band.
    xy = float(np.spacing(np.float32(extent))) / 2 * np.sqrt(2) + errors[0] + BAND_M
    z = float(np.spacing(np.float32(height))) / 2 + errors[1]
    xy += 128 * np.finfo(float).eps * max(1, extent)
    z += 128 * np.finfo(float).eps * max(1, height)
    if not np.isfinite([xy, z]).all() or max(xy, z) > 0.005:
        raise OccupancyError("Derived local construction guard exceeds5mm")
    return float(np.nextafter(xy, np.inf)), float(np.nextafter(z, np.inf))


class RoundingGuardQuery:
    def __init__(self, physical, errors):
        self.physical, self.errors = physical, errors

    def conflict(self, footprint, *, method="index", maximum=8192, envelope=False):
        xy, z = margins(footprint, self.errors)
        expanded = BuildingFootprint(
            footprint.source_id,
            footprint.geometry.buffer(xy, join_style="mitre"),
            HeightInterval(
                footprint.heights.lower_m - z, footprint.heights.upper_m + z
            ),
        )
        extra, work = self.physical.conflict(
            expanded, method=method, maximum=maximum, envelope=envelope
        )
        extra = polygonal_conflict(
            extra.buffer(xy, join_style="mitre").intersection(footprint.geometry)
        )
        return extra, {
            **work,
            "derived_xy_guard_m": xy,
            "derived_z_guard_m": z,
        }


def planning(products, *, prepared=None, baseline=None):
    if (prepared is None) == (baseline is None):
        raise ValueError(
            "Exactly one prepared catalog or full reference baseline required"
        )
    if baseline is not None:
        physical = RailReference(baseline, products)
    else:
        physical = CandidateQuery(prepared)
    return RoundingGuardQuery(physical, round_bounds(products))
