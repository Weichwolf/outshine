"""Direct display subdivision using piecewise upper-surface error envelopes."""

from itertools import pairwise

import numpy as np


def quad_bound(length, second, across_delta, direction_change):
    return length * length * second / 8 + across_delta * direction_change / 4


def envelope(curve, start, end, endpoints=None):
    middle = curve.stations[(curve.stations > start) & (curve.stations < end)]
    stations = np.r_[start, middle, end]
    points = curve.at(stations)
    ends = points[[0, -1]] if endpoints is None else endpoints
    parameter = (stations - start) / (end - start)
    linear = ends[0] + parameter[:, None, None] * (ends[1] - ends[0])
    residual = np.linalg.norm(points - linear, axis=2).max(axis=1)
    indices = np.minimum(
        np.searchsorted(
            curve.stations, (stations[:-1] + stations[1:]) / 2, side="right"
        )
        - 1,
        len(curve.lengths) - 1,
    )
    local = np.diff(stations) ** 2 * curve.second[indices] / 8
    longitudinal = float(np.max(np.maximum(residual[:-1], residual[1:]) + local))
    twist = float(
        np.linalg.norm(
            np.diff(ends[1], axis=0) - np.diff(ends[0], axis=0), axis=1
        ).max()
        / 4
    )
    return longitudinal + twist + curve.evaluation_error_bound_m, ends, len(stations)


def limits(error_m, max_stations):
    if (
        not np.isfinite(error_m)
        or error_m <= 0
        or type(max_stations) is not int
        or not 2 <= max_stations <= 262144
    ):
        raise ValueError("Positive metric error and bounded station capacity required")


def qualified(curve, error_m, max_stations):
    limits(error_m, max_stations)
    if not np.isfinite(curve.evaluation_error_bound_m) or (
        curve.evaluation_error_bound_m >= error_m
    ):
        raise ValueError("Original-span quadrature/pose error exceeds surface budget")


def prepare(curve, error_m, max_stations=262144):
    qualified(curve, error_m, max_stations)
    pending = [(float(curve.stations[0]), float(curve.stations[-1]))]
    evaluations_before = curve.evaluations
    accepted = []
    queries = 0
    while pending:
        start, end = pending.pop()
        bound, points, work = envelope(curve, start, end)
        queries += work
        if queries > 4_000_000:
            raise RuntimeError("Direct road display query capacity exceeded")
        quantization = float(
            np.linalg.norm(points - points.astype(np.float32), axis=2).max()
        )
        if bound + quantization <= error_m:
            accepted.append((start, end, bound, points, quantization))
            continue
        if len(pending) + len(accepted) + 2 >= max_stations:
            raise RuntimeError("Direct road display work/station capacity exceeded")
        middle = (start + end) / 2
        if not start < middle < end:
            raise RuntimeError("Road display subdivision cannot advance")
        pending.extend(((middle, end), (start, middle)))
    accepted.sort(key=lambda item: item[0])
    return assembled(curve, accepted, evaluations_before, queries)


def prepare_uniform(curve, error_m, max_stations=262144):
    """First qualifying whole-curve dyadic grid, using the same surface bound."""
    qualified(curve, error_m, max_stations)
    evaluations_before, queries, intervals = curve.evaluations, 0, 1
    while True:
        grid = np.linspace(curve.stations[0], curve.stations[-1], intervals + 1)
        if np.any(np.diff(grid) <= 0):
            raise RuntimeError("Uniform road display subdivision cannot advance")
        accepted = []
        for start, end in pairwise(grid):
            bound, points, work = envelope(curve, start, end)
            queries += work
            if queries > 4_000_000:
                raise RuntimeError("Direct road display query capacity exceeded")
            quantization = float(
                np.linalg.norm(points - points.astype(np.float32), axis=2).max()
            )
            if bound + quantization > error_m:
                break
            accepted.append((start, end, bound, points, quantization))
        if len(accepted) == intervals:
            plan = assembled(curve, accepted, evaluations_before, queries)
            plan["whole_grid_levels"] = intervals.bit_length()
            return plan
        if 2 * intervals + 1 > max_stations:
            raise RuntimeError("Direct road display work/station capacity exceeded")
        intervals *= 2


def assembled(curve, accepted, evaluations_before, queries):
    stations = np.array([a[0] for a in accepted] + [accepted[-1][1]])
    vertices = np.concatenate(
        [a[3][:1] for a in accepted] + [accepted[-1][3][1:]]
    ).astype(np.float32)
    bands = (
        np.array([1], dtype=np.uint32)
        if curve.shoulder == 0
        else np.arange(3, dtype=np.uint32)
    )
    lower = np.arange(len(stations) - 1, dtype=np.uint32)[:, None] * 4 + bands
    indices = np.stack(
        (lower, lower + 4, lower + 1, lower + 1, lower + 4, lower + 5), axis=-1
    ).reshape(-1, 3)
    return {
        "stations_m": stations,
        "positions_enh_m": vertices,
        "indices": indices,
        "maximum_quadrature_remainder_m": float(curve.integration_bounds_m.max()),
        "maximum_source_endpoint_disagreement_m": curve.endpoint_disagreement_m,
        "evaluation_error_bound_m": curve.evaluation_error_bound_m,
        "source_qualification_pose_evaluations": len(curve.lengths),
        "source_qualification_maximum_recurrence_steps": 31 * len(curve.lengths),
        "maximum_model_bound_m": max(a[2] for a in accepted),
        "maximum_stored_bound_m": max(a[2] + a[4] for a in accepted),
        "maximum_vertex_quantization_m": max(a[4] for a in accepted),
        "pose_evaluations": curve.evaluations - evaluations_before,
        "envelope_pose_tests": queries,
        "accepted_intervals": len(accepted),
    }
