"""Geodetic vertical queries of a qualified P1 field before render detail.

The line is the existing WGS84 Float64 sea point + height * geodetic Up.
Predicates refer to those stored binary coordinates and the original source
polygons. Bounds certify arithmetic, not observed hydrology or Earth accuracy.
"""

from dataclasses import dataclass
from fractions import Fraction

import numpy as np
from outshine_ref.geometry.sampling import owned
from outshine_ref.places.camera import axes, ecef
from water_cell_field import CellField
from water_geographic_predicates import (
    Interval,
    coefficients,
    enclose_fraction,
    exact_hit,
    filtered_hit,
    parameters,
    source_members,
)
from water_nodal_height import cell_faces, parameter_triangle


class PredicateBudget(Exception):
    pass


class Work:
    def __init__(self, limit):
        self.limit, self.total, self.counts = limit, 0, {}

    def tick(self, name):
        if self.total == self.limit:
            raise PredicateBudget
        self.total += 1
        self.counts[name] = self.counts.get(name, 0) + 1


@dataclass
class Candidate:
    root: int
    triangle: np.ndarray
    parameter_triangle: np.ndarray
    origin: np.ndarray
    direction: np.ndarray
    work: Work
    exact_values: tuple | None = None

    def exact(self):
        if self.exact_values is None:
            self.work.tick("exact_root_tests")
            self.exact_values = coefficients(
                self.triangle, self.origin, self.direction, Fraction
            )
        return self.exact_values

    def exact_point(self):
        return parameters(self.exact(), self.parameter_triangle, Fraction)


@dataclass(frozen=True)
class GeographicSamples:
    states: tuple
    height_m: np.ndarray
    height_bounds_m: np.ndarray
    world: np.ndarray
    normals: np.ndarray
    parameter: np.ndarray
    parents: np.ndarray
    source_offsets: np.ndarray
    source_members: np.ndarray
    report: dict


def root_candidate(candidate, polygons, *, exact):
    candidate.work.tick("root_tests")
    if exact:
        values = candidate.exact()
        if not exact_hit(values):
            return None
    else:
        values = coefficients(
            candidate.triangle, candidate.origin, candidate.direction, Interval.value
        )
        inside = filtered_hit(values)
        if inside is False:
            return None
        if inside is None:
            values = candidate.exact()
            if not exact_hit(values):
                return None
    if candidate.exact_values is not None:
        point = tuple(enclose_fraction(x) for x in candidate.exact_point())
        height = enclose_fraction(candidate.exact()[4] / candidate.exact()[0])
    else:
        point = parameters(values, candidate.parameter_triangle, Interval.value)
        height = values[4] / values[0]
    members = source_members(
        polygons, point, candidate.exact_point, candidate.work.tick, exact=exact
    )
    if not members:
        return None
    return candidate, height, point, members


def select_hit(hits):
    if len(hits) == 1:
        return hits[0]
    # Edge/corner ties use exact line coefficients and then the canonical root.
    return max(hits, key=lambda h: (h[0].exact()[4] / h[0].exact()[0], -h[0].root))


def query_geographic(
    field, lon_lat, *, exact=False, predicate_limit=1048576, member_limit=100000
):
    points = np.asarray(lon_lat, dtype=float)
    if (
        not isinstance(field, CellField)
        or points.ndim != 2
        or points.shape[1] != 2
        or not 0 < len(points) <= 4096
        or not np.isfinite(points).all()
        or np.any(abs(points[:, 0]) > 180)
        or np.any(abs(points[:, 1]) > 90)
        or not np.isfinite(field.world).all()
        or abs(field.world).max() > 1e8
        or type(exact) is not bool
        or type(predicate_limit) is not int
        or not 0 <= predicate_limit <= 16777216
        or type(member_limit) is not int
        or not 0 <= member_limit <= 100000
    ):
        raise ValueError(
            "qualified bounded field and finite WGS84 longitude/latitude queries required"
        )
    n = len(points)
    heights, bounds = np.full(n, np.nan), np.full((n, 2), np.nan)
    world, normals, parameter = (
        np.full((n, 3), np.nan),
        np.full((n, 3), np.nan),
        np.full((n, 2), np.nan),
    )
    parents, offsets = np.full(n, -1, np.int16), np.zeros(n + 1, np.uint32)
    states, members = [], []
    work = Work(predicate_limit)
    faces = cell_faces(field.cell)
    origins = ecef(points[:, 1], points[:, 0], 0)
    directions = [axes(lat, lon)[2] for lon, lat in points]
    for i, (origin, direction) in enumerate(zip(origins, directions, strict=True)):
        offsets[i + 1] = len(members)
        try:
            hits = []
            for at, root in enumerate(field.roots):
                candidate = Candidate(
                    int(root),
                    field.world[at],
                    parameter_triangle(faces[int(root)]) - field.cell[1:],
                    origin,
                    direction,
                    work,
                )
                hit = root_candidate(candidate, field.polygons, exact=exact)
                if hit is not None:
                    hits.append(hit)
            if not hits:
                states.append("miss")
                continue
            candidate, height, point, sources = select_hit(hits)
        except PredicateBudget:
            states.append("predicate_budget_exhausted")
            continue
        if len(members) + len(sources) > member_limit:
            states.append("membership_budget_exhausted")
            continue
        h = height.midpoint()
        heights[i], bounds[i] = h, (height.low, height.high)
        world[i] = origin + h * direction
        at = int(np.flatnonzero(field.roots == candidate.root)[0])
        normals[i], parents[i] = field.normals[at], candidate.root
        parameter[i] = [p.midpoint() for p in point]
        members.extend(sources)
        offsets[i + 1] = len(members)
        states.append("water")
    arrays = (
        heights,
        bounds,
        world,
        normals,
        parameter,
        parents,
        offsets,
        np.asarray(members, dtype=np.uint32),
    )
    report = dict(
        work.counts,
        predicates=work.total,
        committed_members=len(members),
        output_array_bytes=sum(a.nbytes for a in arrays),
        geometry_workspace_bytes=None,
    )
    return GeographicSamples(
        tuple(states), *(owned(a, a.dtype) for a in arrays), report
    )
