"""Plan free soil refinement before comparing objectives on the same fixed mesh."""

import numpy as np
import shapely
from stair_blend_constraints import refine, violating_faces
from stair_blend_solve import solve
from stair_terrain_layout import partition, selected_surfaces


def prepare(data, rows, context, raster, sample):
    ground, upper = selected_surfaces(data, rows)
    footprint = shapely.union_all(
        shapely.polygons(ground.vertices[ground.triangles, :2])
    )
    initial, note = partition(ground, context, raster)
    heights, _roles, probe = solve(initial, None, footprint)
    if heights is None:
        raise ValueError("bounded refinement probe failed: " + str(probe))
    bad = violating_faces(initial, heights, upper, footprint)
    if bad:
        layout, refinement = refine(initial, bad, sample)
    else:
        layout = dict(initial, parent_faces=np.arange(len(initial["faces"])))
        refinement = {"refined_cells": 0, "edge_midpoints": 0}
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
