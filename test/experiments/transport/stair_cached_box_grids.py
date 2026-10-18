"""Explicit outward new pad boxes bound to the immutable cached source model."""

import numpy as np
import shapely

GRID_M = 2**-11


def bind_grids(state, recipe, sources, contacts):
    declared = recipe.get("cached_box_grids_m", {})
    if not isinstance(declared, dict) or len(declared) > 5:
        raise ValueError("bounded explicit cached source grid bindings required")
    if not declared:
        return {}
    if (
        recipe.get("contact_mode") != "profile"
        or recipe.get("junction_mode") != "shared_box"
    ):
        raise ValueError("cached box grids require explicit profile/shared_box")
    port = recipe["port"]
    root = tuple(
        sources[port["source_id"]]["mvt_xy"][port.get("vertex", port.get("end"))]
    )
    nodes = {port["source_id"]: root}
    for contact in contacts:
        key, node = contact["cached_source_id"], tuple(contact["mvt_xy"])
        if key in nodes and nodes[key] != node:
            raise ValueError("one cached grid anchor per declared source required")
        nodes[key] = node
    result = {}
    for key, grid in declared.items():
        row = state.get("cached_module_rows", {}).get(key)
        if (
            key not in nodes
            or grid != GRID_M
            or row is None
            or row.get("source_grid_m") != grid
            or not np.array_equal(row["xy"], sources[key]["xy_m"])
        ):
            raise ValueError("box grid must match its verified cached original source")
        result[nodes[key]] = grid
    return result


def outward_box(shape, grid):
    bounds = np.array(shape.bounds)
    if grid != GRID_M or not np.isfinite(bounds).all() or abs(bounds).max() >= 8190:
        raise ValueError("declared local cached grid box range required")
    quantized = np.r_[np.floor(bounds[:2] / grid), np.ceil(bounds[2:] / grid)] * grid
    result = shapely.box(*quantized)
    if not result.covers(shape) or result.hausdorff_distance(shape) > np.sqrt(2) * grid:
        raise ValueError("cached box must retain its whole outward metric contract")
    return result
