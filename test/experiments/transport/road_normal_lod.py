"""Bounded conforming tensor refinement using whole position and normal cells."""

from itertools import pairwise

import numpy as np
from road_normal_cone import NormalModel
from road_surface_lod import envelope, qualified


class NormalBudget(ValueError):
    def __init__(self, message, queries, vertices, requests=None):
        super().__init__(message)
        self.work = {"normal_queries": queries, "predicted_vertices": vertices}
        if requests is not None:
            self.work["normal_requests"] = requests


def prepare(
    curve, error_m, angle_rad, *, uniform=False, max_vertices=65536, max_queries=16384
):
    qualified(curve, error_m, 262144)
    if (
        not np.isfinite(angle_rad)
        or not 0 < angle_rad <= 1
        or type(max_vertices) is not int
        or not 4 <= max_vertices <= 262144
        or type(max_queries) is not int
        or not 1 <= max_queries <= 262144
    ):
        raise ValueError(
            "Bounded geometry, normal angle, vertex and query limits required"
        )
    model = NormalModel(curve)
    s = np.array(curve.stations)
    u = np.unique(curve.across)
    queries = 0
    requests = 0
    hits = 0
    # The curve and angle are immutable for this call. Records never outlive it.
    cells = {}
    rounds = 0
    positions_before = curve.evaluations

    def cone(index, a, b, c, d):
        nonlocal queries, requests, hits
        if requests >= 4 * max_queries:
            raise NormalBudget(
                "Whole road normal request budget exhausted; retain original",
                queries,
                (len(s) + len(curve.lengths) - 1) * len(u),
                requests,
            )
        requests += 1
        key = (index, a, b, c, d)
        if key in cells:
            hits += 1
            return cells[key]
        if queries >= max_queries:
            raise NormalBudget(
                "Whole road normal query budget exhausted; retain original",
                queries,
                (len(s) + len(curve.lengths) - 1) * len(u),
                requests,
            )
        queries += 1
        value = model.cone(index, a, b, c, d, angle_rad)
        cells[key] = value
        return value

    while True:
        if (len(s) + len(curve.lengths) - 1) * len(u) > max_vertices:
            raise NormalBudget(
                "Whole road normal vertex budget exhausted; retain original",
                queries,
                (len(s) + len(curve.lengths) - 1) * len(u),
                requests,
            )
        split_s = set()
        split_u = set()
        maximum_position = 0.0
        for a, b in pairwise(s):
            middle = (a + b) / 2
            if not a < middle < b:
                raise ValueError(
                    "Road normal subdivision cannot advance; retain original"
                )
            index = int(np.searchsorted(curve.stations, middle, side="right") - 1)
            bound, points, _work = envelope(curve, a, b)
            stored = bound + float(
                np.linalg.norm(points - points.astype(np.float32), axis=2).max()
            )
            maximum_position = max(maximum_position, stored)
            if stored > error_m:
                split_s.add(middle)
                continue
            for c, d in pairwise(u):
                result = cone(index, a, b, c, d)
                if result["accepted"]:
                    continue
                across = (c + d) / 2
                longitudinal = max(
                    cone(index, a, middle, c, d)["score"],
                    cone(index, middle, b, c, d)["score"],
                )
                lateral = max(
                    cone(index, a, b, c, across)["score"],
                    cone(index, a, b, across, d)["score"],
                )
                if longitudinal <= lateral:
                    split_s.add(middle)
                else:
                    split_u.add(across)
        if not split_s and not split_u:
            break
        if uniform:
            if split_s:
                split_s.update((a + b) / 2 for a, b in pairwise(s))
            if split_u:
                split_u.update((a + b) / 2 for a, b in pairwise(u))
        s = np.unique(np.r_[s, list(split_s)])
        u = np.unique(np.r_[u, list(split_u)])
        rounds += 1
    result = assemble(model, s, u, angle_rad)
    result.update(
        {
            "maximum_stored_position_bound_m": maximum_position,
            "normal_limit_rad": angle_rad,
            "whole_normal_queries": queries,
            "whole_normal_requests": requests,
            "normal_cell_reuses": hits,
            "normal_cell_records": len(cells),
            "maximum_normal_requests": 4 * max_queries,
            "refinement_rounds": rounds,
            "source_pose_evaluations": curve.evaluations - positions_before,
            "normal_arithmetic_work": dict(model.work),
            "normal_model": "exact encoded span coefficients and positive normalized barycentric interpolation",
            "scope": "No physical-profile/GPU/BRDF/World/device admission; source seams stay one-sided.",
        }
    )
    return result


def assemble(model, s, u, angle):
    curve = model.curve
    global_positions = curve.at(s, lateral_m=u).astype(np.float32)
    positions = []
    normals = []
    charts = []
    rows = np.zeros((len(s) - 1, 2), dtype=np.uint32)
    offset = 0
    for index, (start, end) in enumerate(pairwise(curve.stations)):
        begin = int(np.searchsorted(s, start))
        finish = int(np.searchsorted(s, end))
        chosen = s[begin : finish + 1]
        xyz = global_positions[begin : finish + 1].reshape(-1, 3)
        normals.append(
            np.array(
                [
                    model.stored_normal(index, float(a), float(b), angle)
                    for a in chosen
                    for b in u
                ],
                dtype=np.float32,
            )
        )
        charts.append(
            np.stack(np.meshgrid(chosen, u, indexing="ij"), axis=-1).reshape(-1, 2)
        )
        positions.append(xyz)
        for local in range(len(chosen) - 1):
            rows[begin + local] = offset + np.array([local, local + 1]) * len(u)
        offset += len(xyz)
    base = rows[:, 0, None] + np.arange(len(u) - 1, dtype=np.uint32)
    upper = rows[:, 1, None] + np.arange(len(u) - 1, dtype=np.uint32)
    faces = np.stack(
        (base, upper, base + 1, base + 1, upper, upper + 1), axis=-1
    ).reshape(-1, 3)
    return {
        "stations_m": s,
        "lateral_m": u,
        "positions_enh_m": np.concatenate(positions),
        "normals_enh": np.concatenate(normals),
        "station_lateral_m": np.concatenate(charts),
        "indices": faces,
        "cell_vertex_rows": rows,
    }


def sample(plan, stations, lateral):
    s, u = plan["stations_m"], plan["lateral_m"]
    stations, lateral = np.asarray(stations), np.asarray(lateral)
    if (
        not np.isfinite(stations).all()
        or not np.isfinite(lateral).all()
        or np.any(stations < s[0])
        or np.any(stations > s[-1])
        or np.any(lateral < u[0])
        or np.any(lateral > u[-1])
    ):
        raise ValueError("Samples inside the complete selected road domain required")
    i = np.clip(np.searchsorted(s, stations, side="right") - 1, 0, len(s) - 2)
    j = np.clip(np.searchsorted(u, lateral, side="right") - 1, 0, len(u) - 2)
    a = ((stations - s[i]) / (s[i + 1] - s[i]))[:, None, None]
    b = ((lateral - u[j]) / (u[j + 1] - u[j]))[None, :, None]
    low, high = plan["cell_vertex_rows"][i].T
    n = plan["normals_enh"].astype(float)
    p00, p01 = n[low[:, None] + j], n[low[:, None] + j + 1]
    p10, p11 = n[high[:, None] + j], n[high[:, None] + j + 1]
    first = (1 - a - b) * p00 + a * p10 + b * p01
    second = (1 - b) * p10 + (1 - a) * p01 + (a + b - 1) * p11
    result = np.where(a + b <= 1, first, second)
    return result / np.linalg.norm(result, axis=-1, keepdims=True)
