"""Bounded source-height midpoint indicator, before a height mesh or solver exists.

This is an approximation heuristic, not a bound on unsampled source targets or
on the constrained fitted field. The independent P1 overlap comparison is required.
"""

import numpy as np
import shapely


def refine_boundary(
    body, sample_up, tolerance_m, *, rounds=8, vertex_limit=4096, query_limit=16384
):
    if not np.isfinite(tolerance_m) or tolerance_m <= 0:
        raise ValueError("positive source-height indicator tolerance required")
    if rounds is not None and (type(rounds) is not int or not 1 <= rounds <= 8):
        raise ValueError("bounded midpoint rounds required")
    if (
        type(vertex_limit) is not int
        or type(query_limit) is not int
        or not 3 <= vertex_limit <= 8192
        or not 3 <= query_limit <= 65536
    ):
        raise ValueError("bounded source sampling capacities required")
    rings, parts = [], []
    for part in shapely.get_parts(body):
        ids = []
        for ring in (part.exterior, *part.interiors):
            ids.append(len(rings))
            rings.append(np.asarray(ring.coords))
        parts.append(ids)
    queries = sum(len(r) for r in rings)
    planned = queries - len(rings)
    if queries > query_limit or planned > vertex_limit:
        raise ValueError("original boundary exceeds source sampling budget")
    values = np.asarray(sample_up(np.concatenate(rings)), float)
    if values.shape != (queries,) or not np.isfinite(values).all():
        raise ValueError("unresolved original source heights")
    pending, accepted, offset = [], [], 0
    for ri, ring in enumerate(rings):
        h = values[offset : offset + len(ring)]
        offset += len(ring)
        pending.extend(
            (ri, edge, 0.0, 1.0, a, b, ha, hb, 0)
            for edge, (a, b, ha, hb) in enumerate(
                zip(ring[:-1], ring[1:], h[:-1], h[1:], strict=True)
            )
        )
    # Every split adds a boundary vertex; the vertex/query caps also bound passes when rounds=None.
    work = {
        "original_boundary_vertices": planned,
        "split_round_limit": rounds,
        "height_point_queries": queries,
        "split_edges": 0,
        "passes": 0,
        "max_accepted_midpoint_residual_m": 0.0,
    }
    while pending:
        if queries + len(pending) > query_limit:
            raise ValueError("source midpoint query budget exhausted")
        xy = np.asarray([(p[4] + p[5]) * 0.5 for p in pending])
        heights = np.asarray(sample_up(xy), float)
        if heights.shape != (len(xy),) or not np.isfinite(heights).all():
            raise ValueError("unresolved source midpoint heights")
        queries += len(xy)
        work["passes"] += 1
        children = []
        for p, point, height in zip(pending, xy, heights, strict=True):
            ri, edge, lo, hi, a, b, ha, hb, depth = p
            residual = abs(height - (ha + hb) * 0.5)
            if residual <= tolerance_m:
                accepted.append((ri, edge, lo, a))
                work["max_accepted_midpoint_residual_m"] = max(
                    work["max_accepted_midpoint_residual_m"], float(residual)
                )
                continue
            if (
                (rounds is not None and depth >= rounds)
                or planned >= vertex_limit
                or np.array_equal(point, a)
                or np.array_equal(point, b)
            ):
                raise ValueError(
                    f"source midpoint refinement budget exhausted: depth={depth}, vertices={planned}, queries={queries}, residual_m={residual:.17g}, xy={point.tolist()}"
                )
            planned += 1
            work["split_edges"] += 1
            mid = (lo + hi) * 0.5
            children.extend(
                (
                    (ri, edge, lo, mid, a, point, ha, height, depth + 1),
                    (ri, edge, mid, hi, point, b, height, hb, depth + 1),
                )
            )
        pending = children
    grouped = [[] for _ in rings]
    for ri, _edge, _lo, point in sorted(accepted, key=lambda r: r[:3]):
        grouped[ri].append(point)
    result = []
    for ids in parts:
        result.append(shapely.Polygon(grouped[ids[0]], [grouped[i] for i in ids[1:]]))
    geometry = result[0] if len(result) == 1 else shapely.MultiPolygon(result)
    if not geometry.is_valid or geometry.is_empty:
        raise ValueError("source sampling changed polygon validity")
    difference = float(body.symmetric_difference(geometry).area)
    if difference > max(1e-8, body.area * 1e-12):
        raise ValueError("source sampling changed water coverage")
    return geometry, dict(
        work,
        height_point_queries=queries,
        output_boundary_vertices=planned,
        symmetric_difference_area_m2=difference,
        tolerance_m=tolerance_m,
        scope="finite midpoint indicator; no unsampled target/fit error certificate",
    )
