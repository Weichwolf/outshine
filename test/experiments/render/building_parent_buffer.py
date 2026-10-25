"""Compare scalar expansion with unioned per-component positive buffers."""

import math

import shapely

RECIPES = {
    "scalar": b"building-tree-recipe-1",
    "components": b"building-tree-component-expansion-1",
}


def expand(domain, radius, path="scalar"):
    if path not in RECIPES:
        raise ValueError("known parent buffer path required")
    if not math.isfinite(radius) or radius <= 0:
        raise ValueError("positive parent expansion radius required")
    # Conservative scheduling heuristic, not an optimal or quality threshold.
    if path == "components" and shapely.get_num_geometries(domain) >= 256:
        expanded = shapely.buffer(
            shapely.get_parts(domain), radius, join_style="mitre", mitre_limit=2
        )
        return shapely.union_all(expanded)
    return shapely.buffer(domain, radius, join_style="mitre", mitre_limit=2)
