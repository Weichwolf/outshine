"""Lazy coarse-to-fine upper-surface products from analytic recipes, without Fine IO."""

from itertools import pairwise

import numpy as np
from road_surface_lod import assembled, envelope, qualified


def products(curve, errors_m, *, max_stations=262144, max_queries=4_000_000):
    errors = tuple(errors_m)
    if (
        not errors
        or not all(np.isfinite(error) and error > 0 for error in errors)
        or any(b >= a for a, b in pairwise(errors))
        or type(max_queries) is not int
        or max_queries <= 0
    ):
        raise ValueError(
            "Finite strictly decreasing errors and a positive query budget required"
        )
    frontier = [(float(curve.stations[0]), float(curve.stations[-1]), None)]
    total_queries, total_evaluations = 0, 0
    for level, error in enumerate(errors):
        qualified(curve, error, max_stations)
        pending = list(reversed(frontier))
        accepted, queries, reused = [], 0, 0
        before = curve.evaluations
        while pending:
            start, end, known = pending.pop()
            if known is None:
                required = (
                    int(np.searchsorted(curve.stations, end, side="left"))
                    - int(np.searchsorted(curve.stations, start, side="right"))
                    + 2
                )
                if required > max_queries - total_queries:
                    raise RuntimeError(
                        "Road LOD chain exceeds its cumulative query budget"
                    )
                bound, points, work = envelope(curve, start, end)
                queries += work
                total_queries += work
                rounding = float(
                    np.linalg.norm(points - points.astype(np.float32), axis=2).max()
                )
                known = (start, end, bound, points, rounding)
            else:
                reused += 1
            if known[2] + known[4] <= error:
                accepted.append(known)
                continue
            if len(pending) + len(accepted) + 3 > max_stations:
                raise RuntimeError("Road LOD chain exceeds its station budget")
            middle = (start + end) / 2
            if not start < middle < end:
                raise RuntimeError("Road LOD subdivision cannot advance")
            pending.extend(((middle, end, None), (start, middle, None)))
        total_evaluations += curve.evaluations - before
        frontier = [(item[0], item[1], item) for item in accepted]
        plan = assembled(curve, accepted, before, queries)
        plan.update(
            level=level,
            requested_error_m=error,
            reused_intervals=reused,
            cumulative_envelope_pose_tests=total_queries,
            cumulative_pose_evaluations=total_evaluations,
        )
        yield plan
        del plan
