"""Caller-owned XY box index; actual footprints still decide every distance."""

import time
from dataclasses import dataclass, replace

import numpy as np
import shapely
from stair_occupancy_candidate_distance import (
    FlatDistance,
    footprints,
    nearest,
    selected_distance,
)
from stair_occupancy_geometry import OccupancyError


@dataclass(frozen=True)
class BoxLayout:
    sources: tuple
    bounds: np.ndarray
    ids: np.ndarray
    starts: np.ndarray
    shapes: dict
    tree: object


def prepare(sources, work):
    bounds, ids, starts, known = [], [], [0], {}
    for source in sources:
        for face, box in enumerate(source.bounds):
            identity = starts[-1] + face
            if source.always or source.xy_geometry is None:
                shapes = footprints(source, face, work)
                work["preparation_candidates"] += 1
                if not shapes:
                    continue
                xy = np.asarray([shape.bounds for shape in shapes])
                value = np.r_[xy[:, :2].min(0), xy[:, 2:].max(0)]
                known[len(bounds)] = shapes
            else:
                value = box[[0, 1, 3, 4]]
            bounds.append(value)
            ids.append(identity)
        starts.append(starts[-1] + len(source.bounds))
    if not bounds:
        raise OccupancyError("no verified XY distance found")
    bounds = np.asarray(bounds, float)
    if np.max(abs(bounds)) > 1e6:
        return None
    # A normal epsilon pad keeps degenerate envelopes valid even at zero.
    pad = np.finfo(float).eps * np.maximum(1, abs(bounds))
    bounds[:, :2] = np.nextafter(bounds[:, :2] - pad[:, :2], -np.inf)
    bounds[:, 2:] = np.nextafter(bounds[:, 2:] + pad[:, 2:], np.inf)
    ids, starts = np.asarray(ids, np.int64), np.asarray(starts, np.int64)
    tree = shapely.STRtree(shapely.box(*bounds.T))
    for array in (bounds, ids, starts):
        array.flags.writeable = False
    return BoxLayout(sources, bounds, ids, starts, known, tree)


class BoxDistance:
    def __init__(self):
        self.layout = None
        self.flat = FlatDistance()
        self.sources = None
        self.metadata = None

    def query(self, catalog, geometry, budget):
        if self.sources is not catalog.sources:
            self.layout = None
            self.flat = FlatDistance()
            self.sources = catalog.sources
            self.metadata = None
        if (
            geometry.is_empty
            or not geometry.is_valid
            or not np.isfinite(geometry.bounds).all()
        ):
            raise OccupancyError(
                "distance requires a nonempty valid finite XY geometry"
            )
        if len(catalog.sources) > budget.products:
            raise OccupancyError("distance product budget exhausted")
        if not catalog.sources:
            raise OccupancyError("distance catalog has no verified facets")
        if self.metadata is None:
            count = sum(len(s.bounds) for s in catalog.sources)
            if count > budget.face_tests:
                raise OccupancyError("distance metadata face budget exhausted")
            unknown = sum(
                len(s.bounds)
                for s in catalog.sources
                if s.always or s.xy_geometry is None
            )
            local = all(
                np.max(abs(box[[0, 1, 3, 4]])) <= 1e6 for box in catalog.envelopes
            )
            self.metadata = count, unknown, local
        count, unknown, local = self.metadata
        if count > budget.face_tests:
            raise OccupancyError("distance metadata face budget exhausted")
        cold = self.layout is None
        preparation_faces = count + unknown if cold else 0
        # Charge two full leaf sets plus cold packing and unknown footprints.
        # Internal tree nodes are opaque; this is not an instruction count.
        if (
            2 * count + preparation_faces > budget.face_tests
            or (unknown if cold else 0) + 1 > budget.candidates
            or not local
            or np.max(abs(np.asarray(geometry.bounds))) > 1e6
        ):
            return nearest(catalog, geometry, budget, method="flat", prepared=self.flat)
        work = {
            "products": len(catalog.sources),
            "face_tests": 2 * count,
            "geometry_candidates": 0,
            "volume_materializer_calls": 0,
            "prune_slack_m": 1e-9,
            "index_queries": 0,
            "returned_metadata_boxes": 0,
            "preparation_candidates": 0,
            "preparation_metadata_boxes": 0,
            "face_tests_are_upper_bound": True,
            "tree_internal_visits": None,
            "preparation_cpu_s": 0.0,
            "preparation_wall_s": 0.0,
        }
        try:
            if self.layout is None:
                cpu, wall = time.process_time(), time.perf_counter()
                layout = prepare(catalog.sources, work)
                work["preparation_metadata_boxes"] = count
                work["preparation_cpu_s"] = time.process_time() - cpu
                work["preparation_wall_s"] = time.perf_counter() - wall
                if layout is None:
                    remaining = replace(
                        budget,
                        face_tests=budget.face_tests - preparation_faces,
                        candidates=budget.candidates - work["preparation_candidates"],
                    )
                    value, flat = nearest(
                        catalog, geometry, remaining, method="flat", prepared=self.flat
                    )
                    flat["face_tests"] += preparation_faces
                    flat["preparation_metadata_boxes"] = count
                    flat["preparation_candidates"] = work["preparation_candidates"]
                    flat["volume_materializer_calls"] += work[
                        "volume_materializer_calls"
                    ]
                    flat["preparation_cpu_s"] = work["preparation_cpu_s"]
                    flat["preparation_wall_s"] = work["preparation_wall_s"]
                    return value, flat
                self.layout = layout
                work["face_tests"] += preparation_faces
            value = self.search(geometry, budget, work)
        except (ValueError, shapely.errors.GEOSException) as error:
            raise OccupancyError(
                "bounded indexed XY distance unresolved: " + str(error)
            ) from error
        return value, work

    def search(self, geometry, budget, work):
        layout = self.layout

        def evaluate(index):
            if (
                work["geometry_candidates"] + work["preparation_candidates"]
                >= budget.candidates
            ):
                raise OccupancyError("distance candidate budget exhausted")
            work["geometry_candidates"] += 1
            if index in layout.shapes:
                result = min(float(geometry.distance(s)) for s in layout.shapes[index])
                if not np.isfinite(result) or result < 0:
                    raise OccupancyError("distance geometry unresolved")
                return result
            identity = int(layout.ids[index])
            owner = int(np.searchsorted(layout.starts, identity, side="right") - 1)
            return selected_distance(
                layout.sources[owner],
                identity - int(layout.starts[owner]),
                geometry,
                work,
            )

        first = int(layout.tree.nearest(geometry))
        work["index_queries"] += 1
        best = evaluate(first)
        if not np.isfinite(best):
            raise OccupancyError("no verified XY distance found")
        if best == 0:
            return best
        chosen = layout.tree.query(
            geometry, predicate="dwithin", distance=np.nextafter(best + 1e-9, np.inf)
        )
        work["index_queries"] += 1
        work["returned_metadata_boxes"] = len(chosen)
        box = np.asarray(geometry.bounds)
        delta = np.maximum(
            layout.bounds[chosen, :2] - box[2:], box[:2] - layout.bounds[chosen, 2:]
        )
        lower = np.maximum(0, np.nextafter(delta.max(1), -np.inf))
        for at in np.argsort(lower, kind="stable"):
            if lower[at] > np.nextafter(best + 1e-9, np.inf):
                break
            index = int(chosen[at])
            if index != first:
                best = min(best, evaluate(index))
            if best == 0:
                break
        return best
