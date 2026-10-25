"""One deterministic source-first neighborhood/height grouping and vector closing."""

from collections import defaultdict

import numpy as np
import shapely

GAP_M = 3.0
HEIGHT_SPAN_M = 3.0
MAXIMUM_HEIGHT_M = 12.0


def eligible(source):
    return (
        not source.raised
        and source.roof[1] - source.bottom[0] <= MAXIMUM_HEIGHT_M
        and source.roof[1] - source.roof[0] <= HEIGHT_SPAN_M
        and source.bottom[1] - source.bottom[0] <= HEIGHT_SPAN_M
    )


def group_sources(sources):
    """Nearest edges first; every merged component obeys the full height ranges."""
    if not 0 < len(sources) <= 4096:
        raise ValueError("bounded nonempty original source set required")
    if len({s.source for s in sources}) != len(sources):
        raise ValueError("unique source ordinals required")
    sources = sorted(sources, key=lambda s: s.source)
    geometry = np.array([s.footprint for s in sources], object)
    tree, batches, count = shapely.STRtree(geometry), [], 0
    for begin in range(0, len(geometry), 64):
        pairs = tree.query(
            geometry[begin : begin + 64], predicate="dwithin", distance=GAP_M
        )
        pairs[0] += begin
        pairs = pairs[:, pairs[0] < pairs[1]]
        count += pairs.shape[1]
        if count > 65536:
            raise ValueError("source proximity candidate budget exhausted")
        batches.append(pairs)
    pairs = np.hstack(batches)
    distance = shapely.distance(geometry[pairs[0]], geometry[pairs[1]])
    order = np.lexsort((pairs[1], pairs[0], distance))
    parent = np.arange(len(sources))
    ranges = np.array([[*s.bottom, *s.roof] for s in sources])
    supported = np.array([eligible(s) for s in sources])

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return int(i)

    merges = 0
    for at in order:
        a, b = sorted(root(int(i)) for i in pairs[:, at])
        if a == b or not (supported[a] and supported[b]):
            continue
        low = np.minimum(ranges[a, [0, 2]], ranges[b, [0, 2]])
        high = np.maximum(ranges[a, [1, 3]], ranges[b, [1, 3]])
        if np.any(high - low > HEIGHT_SPAN_M) or high[1] - low[0] > MAXIMUM_HEIGHT_M:
            continue
        parent[b] = a
        ranges[a] = [low[0], high[0], low[1], high[1]]
        merges += 1
    groups = defaultdict(list)
    for i, source in enumerate(sources):
        groups[root(i)].append(source)
    return list(groups.values()), {
        "source_pairs": pairs.shape[1],
        "accepted_merges": merges,
        "eligible_sources": int(supported.sum()),
    }


def close_group(sources):
    if len(sources) < 2:
        raise ValueError("aggregation requires multiple original sources")
    original = shapely.union_all([s.footprint for s in sources])
    holes = shapely.union_all(
        [shapely.Polygon(r) for p in shapely.get_parts(original) for r in p.interiors]
    )
    holes = shapely.difference(holes, original)
    expanded = shapely.buffer(original, GAP_M / 2, join_style="mitre", mitre_limit=2)
    closed = shapely.buffer(expanded, -GAP_M / 2, join_style="mitre", mitre_limit=2)
    domain = shapely.simplify(shapely.difference(closed, holes), 0)
    if (
        domain.is_empty
        or not domain.is_valid
        or shapely.area(shapely.intersection(domain, holes)) > 0
    ):
        raise ValueError("valid nonempty aggregate preserving original holes required")
    return (
        domain,
        (min(s.bottom[0] for s in sources), max(s.roof[1] for s in sources)),
        {
            "source_coordinates": int(shapely.get_num_coordinates(original)),
            "aggregate_coordinates": int(shapely.get_num_coordinates(domain)),
            "added_plan_area_m2": float(
                shapely.area(shapely.difference(domain, original))
            ),
            "removed_plan_area_m2": float(
                shapely.area(shapely.difference(original, domain))
            ),
            "protected_hole_area_m2": float(holes.area),
            "source_components": len(shapely.get_parts(original)),
            "aggregate_components": len(shapely.get_parts(domain)),
        },
    )
