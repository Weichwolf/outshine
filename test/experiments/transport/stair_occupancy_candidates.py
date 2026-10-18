"""Select immutable numeric volume references before creating affine geometry."""

from dataclasses import dataclass, replace

import numpy as np
import shapely
from outshine_ref.contracts import BuildingFootprint, LocalFrame
from stair_occupancy_geometry import OccupancyError
from stair_occupancy_handoff import OccupancyQuery


@dataclass(frozen=True, slots=True)
class CandidateBudget:
    products: int = 256
    face_tests: int = 32768
    candidates: int = 8192

    def __post_init__(self):
        for value, limit in zip(
            (self.products, self.face_tests, self.candidates),
            (2048, 131072, 8192),
            strict=True,
        ):
            if type(value) is not int or not 0 <= value <= limit:
                raise ValueError("explicit bounded candidate work required")


_DEFAULT_BUDGET = CandidateBudget()


@dataclass(frozen=True, slots=True)
class VolumeSource:
    key: str
    bounds: np.ndarray
    materialize: object
    always: bool = False
    xy_geometry: object = None

    def __post_init__(self):
        if (
            not isinstance(self.key, str)
            or not self.key
            or not callable(self.materialize)
            or not isinstance(self.bounds, np.ndarray)
            or self.bounds.dtype != np.dtype(float)
            or self.bounds.ndim != 2
            or self.bounds.shape[1] != 6
            or not 1 <= len(self.bounds) <= 8192
            or not np.isfinite(self.bounds).all()
            or np.any(self.bounds[:, :3] > self.bounds[:, 3:])
            or self.bounds.flags.writeable
            or (self.xy_geometry is not None and not callable(self.xy_geometry))
            or type(self.always) is not bool
        ):
            raise ValueError("bounded immutable numeric volume source required")

    @property
    def envelope(self):
        return np.r_[self.bounds[:, :3].min(0), self.bounds[:, 3:].max(0)]


@dataclass(frozen=True, slots=True)
class CandidateResult:
    volumes: tuple
    source_ids: tuple
    complete: bool
    reason: str
    work: dict


def overlaps(boxes, query):
    return np.all(boxes[:, :3] <= query[3:], axis=1) & np.all(
        boxes[:, 3:] >= query[:3], axis=1
    )


class CandidateVolumes:
    def __init__(self, frame, sources):
        if not isinstance(frame, LocalFrame) or not isinstance(sources, tuple):
            raise TypeError("explicit frame and immutable verified sources required")
        unique = {}
        for source in sources:
            if not isinstance(source, VolumeSource):
                raise TypeError("numeric volume sources required")
            previous = unique.setdefault(source.key, source)
            if previous is not source:
                raise ValueError("ambiguous duplicate source identity")
        if len(unique) > 2048:
            raise ValueError("bounded source catalog required")
        self.frame, self.sources = frame, tuple(unique.values())
        self.envelopes = tuple(s.envelope for s in self.sources)

    def query(self, frame, footprint, *, budget=_DEFAULT_BUDGET, method="coarse"):
        if frame != self.frame or not isinstance(footprint, BuildingFootprint):
            raise ValueError("same named frame and whole typed footprint required")
        if not isinstance(budget, CandidateBudget) or method not in ("flat", "coarse"):
            raise ValueError("explicit bounded numeric selection required")
        work = {
            "products": 0,
            "face_tests": 0,
            "raw_candidates": 0,
            "materializer_calls": 0,
        }

        def absent(reason):
            return CandidateResult((), (), False, reason, work)

        if len(self.sources) > budget.products:
            return absent("product_budget_exhausted")
        x0, y0, x1, y1 = footprint.geometry.bounds
        box = np.array(
            [x0, y0, footprint.heights.lower_m, x1, y1, footprint.heights.upper_m]
        )
        if not np.isfinite(box).all():
            raise ValueError("finite whole-footprint bounds required")
        selected = []
        for source, envelope in zip(self.sources, self.envelopes, strict=True):
            work["products"] += 1
            if (
                method == "coarse"
                and not source.always
                and not overlaps(envelope[None], box)[0]
            ):
                continue
            if work["face_tests"] + len(source.bounds) > budget.face_tests:
                return absent("metadata_face_budget_exhausted")
            work["face_tests"] += len(source.bounds)
            indices = (
                np.arange(len(source.bounds))
                if source.always
                else np.flatnonzero(overlaps(source.bounds, box))
            )
            work["raw_candidates"] += len(indices)
            if work["raw_candidates"] > budget.candidates:
                return absent("candidate_budget_exhausted")
            selected.extend((source, int(i)) for i in indices)
        rows, owners, seen = [], [], {}
        try:
            for source, index in selected:
                produced = source.materialize(index)
                work["materializer_calls"] += 1
                if len(produced) > 1:
                    raise OccupancyError(
                        "one bounded facet may yield at most one volume"
                    )
                for row, keys, _error in produced:
                    if not source.always:
                        q = np.r_[
                            row.geometry.bounds[:2],
                            row.heights.lower_m,
                            row.geometry.bounds[2:],
                            row.heights.upper_m,
                        ]
                        bounds = source.bounds[index]
                        if np.any(q[:3] < bounds[:3]) or np.any(q[3:] > bounds[3:]):
                            raise OccupancyError(
                                "actual volume escapes numeric metadata"
                            )
                    previous = seen.get(row.owner_id)
                    if previous is not None:
                        if previous != (row, keys):
                            raise OccupancyError(
                                "conflicting duplicate volume identity"
                            )
                        continue
                    seen[row.owner_id] = row, keys
                    rows.append(row)
                    owners.append(keys)
        except (ValueError, shapely.errors.GEOSException) as error:
            return absent("volume_materialization_failed: " + str(error))
        return CandidateResult(tuple(rows), tuple(owners), True, "", work)


class CandidateQuery:
    def __init__(
        self,
        catalog,
        *,
        budget=_DEFAULT_BUDGET,
        method="coarse",
        distance_method="flat",
    ):
        self.catalog, self.budget, self.method = catalog, budget, method
        self.distance_method = distance_method

    def nearest_distance(self, geometry):
        from stair_occupancy_candidate_distance import nearest

        return nearest(self.catalog, geometry, self.budget, method=self.distance_method)

    def conflict(self, footprint, *, method="index", maximum=8192, envelope=False):
        if type(maximum) is not int or not 1 <= maximum <= 8192:
            raise ValueError("existing local candidate limit required")
        result = self.catalog.query(
            self.catalog.frame,
            footprint,
            budget=replace(
                self.budget, candidates=min(maximum, self.budget.candidates)
            ),
            method=self.method,
        )
        if not result.complete:
            raise OccupancyError(result.reason)
        if not result.volumes:
            return shapely.Polygon(), {
                "query_rows": 0,
                "xy_candidates": 0,
                "height_candidates": 0,
                "positive_conflicts": 0,
                "seconds": 0.0,
                **{"metadata_" + k: v for k, v in result.work.items()},
            }
        cut, work = OccupancyQuery(result.volumes).conflict(
            footprint, method=method, maximum=maximum, envelope=envelope
        )
        return cut, {**work, **{"metadata_" + k: v for k, v in result.work.items()}}
