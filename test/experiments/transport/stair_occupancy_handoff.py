"""Bounded footprint changes through the existing shared pipeline, not a new API."""

import time

import numpy as np
import shapely
from outshine_ref.contracts import BuildingFootprint, FootprintAdjustment, Reservation
from stair_occupancy_geometry import OccupancyError, polygonal_conflict


class OccupancyQuery:
    def __init__(self, reservations):
        if not isinstance(reservations, tuple) or not 1 <= len(reservations) <= 8192:
            raise ValueError("bounded immutable reservation collection required")
        self.reservations = reservations
        self.shapes = np.array([r.geometry for r in reservations], object)
        self.tree = shapely.STRtree(self.shapes)

    def conflict(self, footprint, *, method="index", maximum=8192, envelope=False):
        if (
            method not in ("index", "scan")
            or type(maximum) is not int
            or not 1 <= maximum <= 8192
        ):
            raise ValueError("explicit bounded query method/capacity required")
        began = time.perf_counter()
        if method == "index":
            chosen = np.sort(
                self.tree.query(footprint.geometry, predicate="intersects")
            )
            visits = len(chosen)
        else:
            chosen = np.flatnonzero(shapely.intersects(self.shapes, footprint.geometry))
            visits = len(self.shapes)
        if len(chosen) > maximum:
            raise OccupancyError(
                "occupancy candidate budget exhausted; footprint must remain deferred"
            )
        cuts, height_pairs = [], 0
        for i in chosen:
            row = self.reservations[i]
            if not row.heights.overlaps(footprint.heights):
                continue
            height_pairs += 1
            shape = (
                row.geometry if envelope else row.conflict_geometry(footprint.heights)
            )
            cut = polygonal_conflict(shape.intersection(footprint.geometry))
            if not cut.is_empty:
                cuts.append(cut)
        return shapely.union_all(cuts), {
            "query_rows": visits,
            "xy_candidates": len(chosen),
            "height_candidates": height_pairs,
            "positive_conflicts": len(cuts),
            "seconds": time.perf_counter() - began,
        }


def adjusted(query, originals, *, method="index", maximum=8192, envelope=False):
    changes, notes = [], []
    for original in originals:
        try:
            mask, cost = query.conflict(
                original, method=method, maximum=maximum, envelope=envelope
            )
        except (OccupancyError, shapely.errors.GEOSException) as error:
            # A local failure cannot expose unverified space to a downstream generator.
            changes.append(
                FootprintAdjustment(
                    original, (), "transport occupancy deferred: " + str(error)
                )
            )
            notes.append({"source_id": original.source_id, "deferred": str(error)})
            continue
        remaining = original.geometry.difference(mask)
        note = dict(
            cost,
            source_id=original.source_id,
            original_area_m2=original.geometry.area,
            retained_area_m2=remaining.area,
        )
        if not remaining.equals(original.geometry):
            parts = (
                ()
                if remaining.is_empty
                else (
                    BuildingFootprint(
                        original.source_id,
                        remaining,
                        original.heights,
                        original.part,
                        original.attributes,
                    ),
                )
            )
            changes.append(
                FootprintAdjustment(
                    original, parts, "actual transport body/headroom overlap"
                )
            )
            note["changed"] = True
        notes.append(note)
    return tuple(changes), notes


def constant_envelopes(reservations):
    return tuple(
        Reservation(r.owner_id, r.geometry, r.heights, r.purpose) for r in reservations
    )
