"""Opt-in binary-grid seed footprints with explicit outward metric bounds."""

import numpy as np
import shapely

GRID_M = 2**-11


def outward_contract(actual, original, bound):
    if (
        actual.geom_type != original.geom_type
        or actual.geom_type != "Polygon"
        or len(actual.interiors) != len(original.interiors)
        or not actual.covers(original)
        or not original.buffer(bound, quad_segs=16).covers(actual)
    ):
        raise ValueError(
            "seed model violates whole outward coverage, topology or metric bound"
        )
    return {
        "outward_bound_m": bound,
        "discrete_hausdorff_m": actual.hausdorff_distance(original),
        "original_holes": len(original.interiors),
    }


def model(shape, grid):
    if grid is None:
        return shape
    if grid != GRID_M or np.max(abs(shapely.get_coordinates(shape)), initial=0) >= 8190:
        raise ValueError("seed binary grid requires the declared local metric range")
    result = shapely.set_precision(
        shape.buffer(2 * grid, join_style="mitre", mitre_limit=2), grid
    )
    result = shapely.set_precision(result, 0)
    outward_contract(result, shape, 6 * grid)
    return result


def node_lines(lines, rows):
    values = {r.get("source_grid_m") for r in rows}
    if len(values) != 1:
        raise ValueError("one consistent declared source grid required")
    grid = values.pop()
    if grid is None:
        return shapely.union_all(lines)
    if grid != GRID_M:
        raise ValueError("known independent seed grid required")
    return shapely.union_all(shapely.set_precision(lines, grid), grid_size=grid)
