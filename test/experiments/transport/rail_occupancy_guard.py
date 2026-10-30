"""Construct extra micrometre room before whole Float32 body admission."""

from dataclasses import replace

import numpy as np
import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval
from rail_occupancy_reference import whole_raised
from stair_occupancy_candidate_reference import PartitionReference
from stair_occupancy_candidates import CandidateQuery, CandidateVolumes
from stair_occupancy_geometry import OccupancyError, polygonal_conflict
from stair_occupancy_handoff import OccupancyQuery


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
    xy = float(np.spacing(np.float32(extent))) / 2 * np.sqrt(2) + errors[0]
    z = float(np.spacing(np.float32(height))) / 2 + errors[1]
    xy += 128 * np.finfo(float).eps * max(1, extent)
    z += 128 * np.finfo(float).eps * max(1, height)
    if not np.isfinite([xy, z]).all() or max(xy, z) > 0.005:
        raise OccupancyError("Derived local construction guard exceeds5mm")
    return float(np.nextafter(xy, np.inf)), float(np.nextafter(z, np.inf))


class RoundingGuardQuery:
    def __init__(self, old, raised, errors):
        self.old, self.raised, self.errors = old, raised, errors

    def conflict(self, footprint, *, method="index", maximum=8192, envelope=False):
        cut, old = self.old.conflict(
            footprint, method=method, maximum=maximum, envelope=envelope
        )
        xy, z = margins(footprint, self.errors)
        expanded = BuildingFootprint(
            footprint.source_id,
            footprint.geometry.buffer(xy, join_style="mitre"),
            HeightInterval(
                footprint.heights.lower_m - z, footprint.heights.upper_m + z
            ),
        )
        query = self.raised
        if isinstance(query, CandidateQuery):
            # Both partitions share the existing total work budget.
            remaining = replace(
                query.budget,
                products=query.budget.products - old["metadata_products"],
                face_tests=query.budget.face_tests - old["metadata_face_tests"],
                candidates=min(maximum, query.budget.candidates)
                - old["metadata_raw_candidates"],
            )
            query = CandidateQuery(query.catalog, budget=remaining)
        extra, work = query.conflict(
            expanded, method=method, maximum=maximum, envelope=envelope
        )
        extra = polygonal_conflict(
            extra.buffer(xy, join_style="mitre").intersection(footprint.geometry)
        )
        return shapely.union_all((cut, extra)), {
            "old": old,
            "raised": work,
            "derived_xy_guard_m": xy,
            "derived_z_guard_m": z,
        }


def planning(products, *, prepared=None, baseline=None):
    if (prepared is None) == (baseline is None):
        raise ValueError(
            "Exactly one prepared catalog or full reference baseline required"
        )
    if baseline is not None:
        old = PartitionReference(baseline, products.state, products.originals)
        raised = OccupancyQuery(whole_raised(products)[0])
    else:
        n = len(products.supports) + 1
        old = CandidateQuery(CandidateVolumes(prepared.frame, prepared.sources[:-n]))
        raised = CandidateQuery(CandidateVolumes(prepared.frame, prepared.sources[-n:]))
    return RoundingGuardQuery(old, raised, round_bounds(products))
