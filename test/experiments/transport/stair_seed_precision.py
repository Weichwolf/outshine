"""Opt-in binary-grid seed footprints with explicit outward metric bounds."""

import numpy as np
import shapely

GRID_M = 2**-11


def model(shape, grid):
    if grid is None:
        return shape
    if grid != GRID_M or np.max(abs(shapely.get_coordinates(shape)), initial=0) >= 8190:
        raise ValueError("seed binary grid requires the declared local metric range")
    result = shapely.set_precision(
        shape.buffer(2 * grid, join_style="mitre", mitre_limit=2), grid
    )
    if (
        result.geom_type != "Polygon"
        or not result.is_valid
        or result.is_empty
        or not result.buffer(1e-9).covers(shape)
        or result.hausdorff_distance(shape) > 6 * grid
    ):
        raise ValueError(
            "canonical seed footprint lost coverage or exceeded its outward bound"
        )
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
