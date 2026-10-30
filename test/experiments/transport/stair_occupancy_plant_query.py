"""Keep actual Buildings parts and bounded Transport demand in separate products."""

import json
from dataclasses import replace
from pathlib import Path

import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import HeightInterval, Reservation
from stair_consumer_inputs import candidate_inputs as data_for
from stair_local_replay import load_chain
from stair_occupancy_candidate_reference import PartitionReference
from stair_occupancy_candidate_reuse import retained
from stair_occupancy_candidate_sources import from_state
from stair_occupancy_candidates import CandidateBudget, CandidateQuery, CandidateVolumes
from stair_occupancy_geometry import OccupancyError
from stair_occupancy_handoff import OccupancyQuery
from stair_occupancy_inputs import actual_sources


def inputs(products, receipt, *, allow_raised=False, state_loader=None):
    report = json.loads((products / "comparison.json").read_text())
    if "elevated_supports" in report and not allow_raised:
        raise ValueError(
            "Raised transport requires the complete elevated consumer adapter"
        )
    checked = json.loads(receipt.read_text())
    if (
        checked["report_sha256"] != digest(products / "comparison.json")
        or not checked[
            "all_source_attributes_roots_heights_and_whole_body_conflicts_checked"
        ]
    ):
        raise ValueError("verified whole current Buildings products required")
    baseline = Path(report["baseline"])
    if digest(baseline / "comparison.json") != report["baseline_report_sha256"]:
        raise ValueError("original verified transport baseline changed")
    for name, expected in report["artifacts"].items():
        path = products / name
        if (
            not path.resolve().is_relative_to(products.resolve())
            or digest(path) != expected
        ):
            raise ValueError("actual complete Buildings artifact changed")
    state = (load_chain if state_loader is None else state_loader)(
        Path(report["resident"]), [Path(p) for p in report["prior_modules"]]
    )
    if (
        report["source_ids"] != state[4]["source_ids"]
        or report["frame"] != state[0].patches[0].frame.identifier
    ):
        raise ValueError("whole Transport source order or named frame changed")
    rows = json.loads((products / "building-reservations.json").read_text())
    if len(rows) != checked["actual_adjusted_bodies"] or [
        r["body_index"] for r in rows
    ] != list(range(len(rows))):
        raise ValueError("whole original building body parts required")
    buildings = tuple(
        Reservation(
            r["owner_id"],
            shapely.from_wkb(r["geometry_wkb"]),
            HeightInterval(*r["heights_m"]),
            r["purpose"],
        )
        for r in rows
    )
    return data_for(baseline, state), state, baseline, buildings


class PlantQuery:
    def __init__(self, transport, buildings):
        self.transport = transport
        self.buildings = OccupancyQuery(buildings) if buildings else None
        self.counts = dict.fromkeys(
            (
                "conflict_calls",
                "distance_calls",
                "distance_face_tests",
                "distance_geometry_candidates",
                "distance_volume_materializer_calls",
                "distance_index_queries",
                "distance_returned_metadata_boxes",
                "distance_preparation_candidates",
                "distance_preparation_metadata_boxes",
                "distance_indexed_calls",
            ),
            0,
        )

    def conflict(self, footprint, **kwargs):
        self.counts["conflict_calls"] += 1
        first, work = self.transport.conflict(footprint, **kwargs)
        if self.buildings is None:
            return first, work
        second, extra = self.buildings.conflict(footprint, **kwargs)
        return shapely.union_all((first, second)), {
            "transport": work,
            "buildings": extra,
        }

    def nearest_distance(self, geometry):
        self.counts["distance_calls"] += 1
        distance, work = self.transport.nearest_distance(geometry)
        for key in (
            "face_tests",
            "geometry_candidates",
            "volume_materializer_calls",
            "index_queries",
            "returned_metadata_boxes",
            "preparation_candidates",
            "preparation_metadata_boxes",
        ):
            self.counts["distance_" + key] += work.get(key, 0)
        if work.get("face_tests_are_upper_bound"):
            self.counts["distance_indexed_calls"] += 1
            self.counts["distance_face_tests_are_upper_bound"] = True
            self.counts["distance_tree_internal_visits"] = None
        if self.buildings is not None:
            index = int(self.buildings.tree.nearest(geometry))
            distance = min(
                distance, float(geometry.distance(self.buildings.shapes[index]))
            )
        return distance, work


def candidate_query(state, buildings, *, extra_sources=()):
    _context, sources, *_ = actual_sources()
    budget = CandidateBudget()
    count = len(buildings)
    if count >= budget.candidates or count >= budget.face_tests:
        raise OccupancyError("building product leaves no bounded Transport capacity")
    budget = replace(
        budget,
        products=budget.products - bool(count),
        face_tests=budget.face_tests - count,
        candidates=budget.candidates - count,
    )
    catalog = from_state(state, sources)
    if extra_sources:
        catalog = CandidateVolumes(catalog.frame, (*catalog.sources, *extra_sources))
    catalog, info = retained(catalog, 8192)
    return PlantQuery(
        CandidateQuery(catalog, budget=budget, distance_method="indexed"), buildings
    ), info


class ReferenceDistance:
    def __init__(self, transport):
        self.transport = transport
        # Oracle only: all unchanged actual volume footprints, no relaxed flat query.
        self.shapes = [r.geometry for r in transport.rows]
        self.tree = shapely.STRtree(self.shapes)

    def nearest_distance(self, geometry):
        index = int(self.tree.nearest(geometry))
        return float(geometry.distance(self.shapes[index])), {}

    def conflict(self, footprint, **kwargs):
        return self.transport.conflict(footprint, **kwargs)


def reference_query(baseline, state, buildings):
    _context, sources, *_ = actual_sources()
    reference = PartitionReference(baseline, state, sources)
    return PlantQuery(ReferenceDistance(reference), buildings)
