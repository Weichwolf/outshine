"""Bounded procedural stair T proposals, separate from exact-source frontiers."""

import math
import time
from itertools import pairwise

from mvt_t_connection_geometry import (
    connection_candidate,
    constructed_axes,
    ground_foot,
    segment_records,
)
from shapely import LineString, STRtree, box


def _checked(rows, max_segments, max_endpoints):
    if len(rows) > max_segments:
        raise ValueError("source cap exceeded")
    segments = sum(len(r["xy_m"]) - 1 for r in rows)
    endpoints = 2 * sum(r["kind"] == "steps" for r in rows)
    if segments > max_segments or endpoints > max_endpoints:
        raise ValueError("segment/endpoint cap exceeded")
    ids = [r["source_id"] for r in rows]
    if any(not isinstance(v, str) or not 1 <= len(v) <= 512 for v in ids):
        raise ValueError("bounded string source-part keys required")
    if len(ids) != len(set(ids)):
        raise ValueError("unique source-part keys required")
    result = []
    for row in rows:
        xy = row["xy_m"]
        if not 2 <= len(xy) <= 4096 or any(
            len(p) != 2 or any(not math.isfinite(v) or abs(v) > 1e7 for v in p)
            for p in xy
        ):
            raise ValueError("bounded finite local XY axes required")
        lengths = [math.dist(a, b) for a, b in pairwise(xy)]
        if min(lengths) < 1e-6:
            raise ValueError("distinct consecutive source vertices required")
        if not math.isfinite(row["half_width_m"]) or not 0 < row["half_width_m"] <= 100:
            raise ValueError("bounded positive half width required")
        resolution = row["position_error_bound_m"]
        if resolution is not None and (
            not math.isfinite(resolution) or not 0 <= resolution <= 100
        ):
            raise ValueError(
                "finite nonnegative supplied position error bound or None required"
            )
        if row["layer"] is not None and type(row["layer"]) is not int:
            raise ValueError("explicit integer layer or None required")
        if any(
            row[k] is not None and type(row[k]) is not bool
            for k in ("bridge", "tunnel", "foot")
        ):
            raise ValueError("explicit tri-state level/access facts required")
        if len(row["endpoint_roles"]) != 2 or any(
            role not in ("connectable", "terminal", "fragment", "unknown")
            for role in row["endpoint_roles"]
        ):
            raise ValueError("explicit endpoint roles required")
        result.append({**row, "length_m": sum(lengths)})
    return sorted(result, key=lambda row: row["source_id"])


def _choose(branch, end, segments, indices, remaining, reach_m, min_sine):
    if len(indices) > remaining:
        return None, "candidate_budget_exhausted", 0
    candidates = []
    for index in sorted(indices):
        candidate = connection_candidate(
            branch, end, segments[index], reach_m, min_sine
        )
        if candidate is not None:
            candidates.append(candidate)
    # Two adjacent target segments may describe the same original target vertex.
    unique = {}
    for candidate in candidates:
        key = (candidate["target_source_id"], tuple(candidate["xy_m"]))
        unique.setdefault(key, candidate)
    candidates = list(unique.values())
    coincident = [c for c in candidates if c["advance_m"] == 0]
    if coincident:
        candidates = coincident
    if len(candidates) != 1:
        return (
            None,
            "ambiguous_targets" if candidates else "no_admissible_target",
            len(indices),
        )
    return candidates[0], "proposed_2d_pending_profiles", len(indices)


def plan_connections(
    rows,
    *,
    reach_m=0.5,
    min_crossing_sine=0.5,
    max_segments=200000,
    max_endpoints=4096,
    max_candidates=100000,
    method="index",
):
    """Return a candidate construction, never native/height admission.

    Reach and minimum crossing angle are explicit recipes. Supplied positional
    position error bounds further limit reach; None remains visibly unqualified.
    Unknown end roles/levels/access and declared terminals/fragments cannot extend.
    The immutable STRtree returns a bounded input-sized array per scalar query;
    operation limits are cooperative between queries, not a hard runtime deadline.
    """
    began = time.perf_counter()
    if method not in ("index", "scan"):
        raise ValueError("index or reference scan required")
    if not math.isfinite(reach_m) or not 0 <= reach_m <= 10:
        raise ValueError("bounded nonnegative extension reach required")
    if not math.isfinite(min_crossing_sine) or not 0 < min_crossing_sine <= 1:
        raise ValueError("positive bounded crossing sine required")
    if any(
        type(v) is not int or not 0 <= v <= 2000000
        for v in (max_segments, max_endpoints, max_candidates)
    ):
        raise ValueError("bounded nonnegative integer caps required")
    checked = _checked(rows, max_segments, max_endpoints)
    targets = [
        r
        for r in checked
        if ground_foot(r) and r["kind"] in ("path", "footway", "pedestrian")
    ]
    segments = segment_records(targets)
    built = time.perf_counter()
    tree = (
        STRtree([LineString((s[2], s[3])) for s in segments])
        if method == "index"
        else None
    )
    indexed = time.perf_counter()
    connections, dispositions = [], []
    work = {
        "segments": sum(len(r["xy_m"]) - 1 for r in checked),
        "target_segments": len(segments),
        "sources": len(rows),
        "index_entries": len(segments) if tree is not None else 0,
        "queries": 0,
        "candidate_indices_returned": 0,
        "candidate_tests": 0,
        "max_query_indices": 0,
        "budget_refusals": 0,
    }
    for branch in checked:
        if branch["kind"] != "steps":
            continue
        for role, end in zip(branch["endpoint_roles"], (0, -1), strict=True):
            reason = "endpoint_role_" + role if role != "connectable" else ""
            if not ground_foot(branch):
                reason = "incompatible_or_unknown_ground_access"
            chosen = None
            if not reason:
                x, y = branch["xy_m"][end]
                indices = (
                    tree.query(
                        box(x - reach_m, y - reach_m, x + reach_m, y + reach_m)
                    ).tolist()
                    if tree is not None
                    else list(range(len(segments)))
                )
                work["queries"] += 1
                work["candidate_indices_returned"] += len(indices)
                work["max_query_indices"] = max(work["max_query_indices"], len(indices))
                chosen, reason, count = _choose(
                    branch,
                    end,
                    segments,
                    indices,
                    max_candidates - work["candidate_tests"],
                    reach_m,
                    min_crossing_sine,
                )
                work["candidate_tests"] += count
                work["budget_refusals"] += reason == "candidate_budget_exhausted"
            if chosen is not None:
                connections.append(chosen)
            dispositions.append(
                {"source_id": branch["source_id"], "endpoint": end, "reason": reason}
            )
    axes = constructed_axes(checked, connections)
    return {
        "status": "proposed_2d_not_height_or_native_admission",
        "connections": connections,
        "axes_xy_m": axes,
        "endpoint_dispositions": dispositions,
        "source_usage": {r["source_id"]: "original_axis_retained" for r in checked},
        "work": work,
        "input_prepare_s": built - began,
        "index_build_s": indexed - built,
        "elapsed_s": time.perf_counter() - began,
        "recipe": {"reach_m": reach_m, "min_crossing_sine": min_crossing_sine},
    }
