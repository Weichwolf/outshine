"""Bounded explicit floor surfaces inside supplied building volumes (#111)."""

import hashlib
from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.contracts import LocalFrame


@dataclass(frozen=True)
class Volume:
    identifier: str
    frame: LocalFrame
    polygon: shapely.Polygon
    lower_m: float
    upper_m: float
    anchor_lon_lat: tuple
    source: dict


@dataclass(frozen=True)
class FloorRequest:
    identifier: str
    frame: LocalFrame
    axis: shapely.LineString
    corridor: shapely.Polygon
    level: object
    original: dict
    purpose: str = "flat_indoor_walkway"


@dataclass(frozen=True)
class FloorLimits:
    headroom_m: float = 2.1
    slab_m: float = 0.2
    nominal_storey_m: float = 3.2
    foundation_deviation_m: float = 2.0
    outline_adjustment_m: float = 0.25
    outline_grid_steps: float = 0.5
    outline_clearance_m: float = 0.001
    max_parts: int = 128
    max_vertices: int = 16384
    max_levels: int = 16
    max_requests: int = 64
    max_source_parts: int = 8192
    max_adjacencies: int = 65536
    max_constraints: int = 1024

    def __post_init__(self):
        values = (
            self.headroom_m,
            self.slab_m,
            self.nominal_storey_m,
            self.foundation_deviation_m,
        )
        if not np.isfinite(values).all() or not all(v > 0 for v in values):
            raise ValueError("Finite positive metric floor parameters required")
        if self.nominal_storey_m < self.headroom_m + 2 * self.slab_m:
            raise ValueError("Nominal storeys must contain floor/roof and clearance")
        if (
            not np.isfinite(
                (
                    self.outline_adjustment_m,
                    self.outline_grid_steps,
                    self.outline_clearance_m,
                )
            ).all()
            or min(
                self.outline_adjustment_m,
                self.outline_grid_steps,
                self.outline_clearance_m,
            )
            < 0
        ):
            raise ValueError(
                "Explicit finite nonnegative outline adaptation bounds required"
            )
        if any(
            type(v) is not int or not 1 <= v <= 65536
            for v in (
                self.max_parts,
                self.max_vertices,
                self.max_levels,
                self.max_requests,
                self.max_source_parts,
                self.max_adjacencies,
                self.max_constraints,
            )
        ):
            raise ValueError(
                "Explicit bounded integer construction capacities required"
            )


@dataclass(frozen=True)
class FloorSurface:
    identifier: str
    frame: LocalFrame
    level: int
    polygon: object
    lower_z_m: float
    upper_z_m: float
    source_ids: tuple


def component(seed, parts, tree, limits, work):
    pending, visited = [seed], {seed}
    vertices = 0
    while pending:
        index = pending.pop()
        vertices += int(shapely.get_num_coordinates(parts[index].polygon))
        if len(visited) > limits.max_parts or vertices > limits.max_vertices:
            return tuple(sorted(visited)), "building_component_budget"
        adjacent = tree.query(parts[index].polygon, predicate="intersects")
        work["component_candidates"] += len(adjacent)
        if work["component_candidates"] > limits.max_adjacencies:
            return tuple(sorted(visited)), "building_adjacency_budget"
        for other in sorted(map(int, adjacent)):
            if other not in visited:
                visited.add(other)
                pending.append(other)
    return tuple(sorted(visited)), ""


def floor_bands(parts, limits, method):
    height = max(p.upper_m for p in parts)
    count = min(
        limits.max_levels,
        max(1, round(height / limits.nominal_storey_m)),
        max(1, int(height / (limits.headroom_m + 2 * limits.slab_m))),
    )
    uniform = tuple(height * i / count for i in range(count))
    if method == "uniform":
        return uniform
    bands = [0.0]
    for low in sorted({p.lower_m for p in parts}):
        if (
            low - bands[-1] >= limits.headroom_m + limits.slab_m
            and height - low >= limits.headroom_m + 2 * limits.slab_m
        ):
            bands.append(low)
    if len(bands) < 2:
        return uniform
    return tuple(bands[: limits.max_levels])


def floor_domain(parts, bottom, limits):
    top = bottom + limits.slab_m
    eligible = [
        p.polygon
        for p in parts
        if p.lower_m <= bottom and p.upper_m >= top + limits.headroom_m + limits.slab_m
    ]
    domain = shapely.union_all(eligible) if eligible else shapely.Polygon()
    obstacles = [
        p.polygon
        for p in parts
        if top <= p.lower_m < top + limits.headroom_m
        or top <= p.upper_m - limits.slab_m < top + limits.headroom_m
    ]
    if obstacles:
        domain = shapely.difference(domain, shapely.union_all(obstacles))
    return domain


def constrained_bands(parts, constraints, limits, work):
    body = shapely.union_all([p.polygon for p in parts])
    local = [
        r
        for r in constraints
        if r.purpose == "flat_indoor_walkway"
        and type(r.level) is int
        and 0 <= r.level < limits.max_levels
        and shapely.intersection(body, r.corridor).area > 0
    ]
    if not local:
        return floor_bands(parts, limits, "structural"), {"static_constraints": 0}
    height = max(p.upper_m for p in parts)
    uniform = floor_bands(parts, limits, "uniform")
    candidates = sorted(
        {
            0.0,
            *uniform,
            *(
                p.lower_m
                for p in parts
                if p.lower_m + limits.headroom_m + 2 * limits.slab_m <= height
            ),
        }
    )
    if len(candidates) > limits.max_levels:
        raise ValueError("floor_candidate_budget")
    domains = [floor_domain(parts, b, limits) for b in candidates]
    work["floor_candidate_domains"] += len(domains)
    levels = min(limits.max_levels, max(len(uniform), max(r.level for r in local) + 1))
    scores = []
    for level in range(levels):
        requests = [r for r in local if r.level == level]
        scores.append(
            [int(sum(d.covers(r.corridor) for r in requests)) for d in domains]
        )
        work["floor_score_tests"] += len(domains) * len(requests)
    # State owns the last selected metric band. Skipping unsupported semantic
    # labels preserves later possibilities; no queried/view-dependent scoring.
    states = {0: ((scores[0][0], int(not domains[0].is_empty), 0, 0), (0.0,))}

    def keep(table, index, score, path):
        if index not in table or score > table[index][0]:
            table[index] = score, path

    for level in range(1, levels):
        following = {}
        for previous in sorted(states):
            score, path = states[previous]
            keep(following, previous, score, path + (None,))
            for candidate in range(previous + 1, len(candidates)):
                work["programme_transitions"] += 1
                if (
                    domains[candidate].is_empty
                    or candidates[candidate] - candidates[previous]
                    < limits.headroom_m + limits.slab_m
                ):
                    continue
                keep(
                    following,
                    candidate,
                    (
                        score[0] + scores[level][candidate],
                        score[1] + 1,
                        score[2] - level,
                        score[3] - candidate,
                    ),
                    path + (candidates[candidate],),
                )
        states = following
    best = max(states.values(), key=lambda item: item[0])
    return best[1], {
        "static_constraints": len(local),
        "candidate_bands_m": candidates,
        "covered_static_constraints": best[0][0],
        "objective": best[0],
        "scope": "Finite candidate/semantic-layer dynamic programme, snapshot source constraints only; not a continuous/global building optimum.",
    }


def construct(parts, levels, ground, limits, method, constraints, work):
    samples = np.asarray(ground(parts), dtype=float)
    if samples.shape != (len(parts),) or not np.isfinite(samples).all():
        raise ValueError("missing_bound_building_elevation")
    base = float(np.median(samples))
    if abs(samples - base).max() > limits.foundation_deviation_m:
        raise ValueError("shared_foundation_exceeds_deviation")
    if method == "constrained":
        bands, selection = constrained_bands(parts, constraints, limits, work)
    else:
        bands, selection = floor_bands(parts, limits, method), {}
    source_ids = tuple(p.identifier for p in parts)
    identity = hashlib.sha256("\n".join(source_ids).encode()).hexdigest()[:20]
    floors = []
    for level in sorted(levels):
        if level >= len(bands) or bands[level] is None:
            continue
        bottom = bands[level]
        domain = floor_domain(parts, bottom, limits)
        if not domain.is_empty:
            floors.append(
                FloorSurface(
                    f"floor-{identity}-{method}-{level}",
                    parts[0].frame,
                    level,
                    domain,
                    base + bottom,
                    base + bottom + limits.slab_m,
                    source_ids,
                )
            )
    return floors, {
        "source_ids": source_ids,
        "bands_m": bands,
        "base_z_m": base,
        "maximum_sampled_foundation_change_m": float(abs(samples - base).max()),
        "selection": selection,
    }


def plan_floors(
    parts, requests, ground, *, limits=None, method="structural", constraints=None
):
    limits = FloorLimits() if limits is None else limits
    if method not in ("structural", "uniform", "constrained"):
        raise ValueError("Known explicit floor programme required")
    if method == "constrained" and constraints is None:
        raise ValueError(
            "Complete fixed snapshot constraints required, independently of current queries"
        )
    constraints = () if constraints is None else constraints
    if len(constraints) > limits.max_constraints:
        raise ValueError("Static indoor constraint admission budget exceeded")
    if len(requests) > limits.max_requests:
        raise ValueError("Floor request admission budget exceeded")
    if len(parts) > limits.max_source_parts:
        raise ValueError("Source building admission budget exceeded")
    frames = {p.frame for p in parts} | {r.frame for r in (*requests, *constraints)}
    if len(frames) != 1:
        raise ValueError("One explicit shared floor/request frame required")
    if (
        len({p.identifier for p in parts}) != len(parts)
        or len({r.identifier for r in requests}) != len(requests)
        or len({r.identifier for r in constraints}) != len(constraints)
    ):
        raise ValueError("Unique source and request identities required")
    parts = tuple(sorted(parts, key=lambda p: p.identifier))
    tree = shapely.STRtree([p.polygon for p in parts])
    work = {
        "query_candidates": 0,
        "component_candidates": 0,
        "ground_samples": 0,
        "floor_candidate_domains": 0,
        "floor_score_tests": 0,
        "programme_transitions": 0,
    }
    seeds, relations = set(), {}
    for request in requests:
        if (
            request.purpose != "flat_indoor_walkway"
            or type(request.level) is not int
            or not 0 <= request.level < limits.max_levels
        ):
            relations[request.identifier] = ()
            continue
        candidates = tuple(
            sorted(map(int, tree.query(request.corridor, predicate="intersects")))
        )
        work["query_candidates"] += len(candidates)
        relations[request.identifier] = candidates
        seeds.update(candidates)
    visited, floors, programmes, failures = set(), [], [], {}
    for seed in sorted(seeds):
        if seed in visited:
            continue
        members, reason = component(seed, parts, tree, limits, work)
        visited.update(members)
        levels = {
            r.level
            for r in requests
            if type(r.level) is int
            and 0 <= r.level < limits.max_levels
            and any(i in members for i in relations[r.identifier])
        }
        if not reason:
            try:
                local = tuple(parts[i] for i in members)
                work["ground_samples"] += len(local)
                built, programme = construct(
                    local, levels, ground, limits, method, constraints, work
                )
                floors.extend(built)
                programmes.append(programme)
            except ValueError as error:
                reason = str(error)
        if reason:
            failures.update({i: reason for i in members})
    choices = []
    for request in requests:
        candidate = [
            f
            for f in floors
            if f.level == request.level and f.polygon.covers(request.corridor)
        ]
        if request.purpose != "flat_indoor_walkway":
            reason = "unplanned_indoor_contact_kind"
        elif (
            type(request.level) is not int or not 0 <= request.level < limits.max_levels
        ):
            reason = "unplanned_semantic_level"
        elif any(i in failures for i in relations[request.identifier]):
            reason = failures[
                next(i for i in relations[request.identifier] if i in failures)
            ]
        elif not relations[request.identifier]:
            reason = "no_source_building_candidate"
        elif len(candidate) != 1:
            reason = (
                "ambiguous_chosen_floor"
                if candidate
                else "chosen_floor_does_not_cover_corridor"
            )
        else:
            reason = ""
        row = {
            "source_id": request.identifier,
            "status": "pending" if reason else "ready",
            "reason": reason,
        }
        if not reason:
            floor = candidate[0]
            row.update(
                floor_id=floor.identifier,
                xyz=np.c_[
                    np.asarray(request.axis.coords),
                    np.full(len(request.axis.coords), floor.upper_z_m),
                ],
            )
        choices.append(row)
    return {
        "floors": tuple(floors),
        "programmes": programmes,
        "choices": choices,
        "work": work,
    }
