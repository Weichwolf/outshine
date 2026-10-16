"""Plan free soil refinement before comparing objectives on the same fixed mesh."""

import numpy as np
import shapely
from stair_blend_constraints import refine, violating_faces
from stair_blend_solve import solve
from stair_terrain_layout import partition, selected_surfaces


def prepare(
    data,
    rows,
    context,
    raster,
    sample,
    *,
    precision="legacy",
    interface="grid",
    variables="heights",
    spacing_m=12.0,
    sampling="uniform",
):
    if sampling not in ("uniform", "transport_neighborhood"):
        raise ValueError("known explicit terrain sampling required")
    ground, upper = selected_surfaces(data, rows)
    footprint = shapely.union_all(
        shapely.polygons(ground.vertices[ground.triangles, :2])
    )
    transport = None
    if sampling == "transport_neighborhood":
        transport = shapely.union_all(
            shapely.polygons(data["body_vertices"][data["top_faces"], :2])
        )
    initial, note = partition(
        ground,
        context,
        raster,
        sample=sample,
        interface=interface,
        spacing_m=spacing_m,
        transport=transport,
    )
    if precision != "legacy":
        initial["precision_mode"] = np.array(precision)
    if variables != "heights":
        initial["variable_mode"] = np.array(variables)
    heights, _roles, probe = solve(initial, None, footprint)
    if heights is None:
        raise ValueError("bounded refinement probe failed: " + str(probe))
    bad = violating_faces(initial, heights, upper, footprint)
    if bad:
        layout, refinement = refine(initial, bad, sample)
    else:
        layout = dict(initial, parent_faces=np.arange(len(initial["faces"])))
        refinement = {"refined_cells": 0, "edge_midpoints": 0}
    if precision != "legacy":
        layout["precision_mode"] = np.array(precision)
    if variables != "heights":
        layout["variable_mode"] = np.array(variables)
    if sampling != "uniform":
        initial["sampling_mode"] = np.array(sampling)
        layout["sampling_mode"] = np.array(sampling)
    return (
        initial,
        layout,
        ground,
        upper,
        footprint,
        {
            "initial_partition": note,
            "refinement_probe": probe,
            "refinement": refinement,
            "probe_scope": "Only selects free soil points; not a bridge-clearance-admitted surface.",
        },
    )
