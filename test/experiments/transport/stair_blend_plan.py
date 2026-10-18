"""Plan free soil refinement before comparing objectives on the same fixed mesh."""

import numpy as np
import shapely
from stair_blend_constraints import refine, violating_faces
from stair_blend_solve import solve
from stair_terrain_layout import partition, selected_surfaces


def refinement_pass(layout, upper, footprint, sample):
    heights, _roles, probe = solve(layout, None, footprint)
    if heights is None:
        raise ValueError("bounded refinement probe failed: " + str(probe))
    bad = violating_faces(layout, heights, upper, footprint)
    if bad:
        result, note = refine(layout, bad, sample)
        if "parent_faces" in layout:
            result["parent_faces"] = layout["parent_faces"][result["parent_faces"]]
    else:
        result = dict(
            layout,
            parent_faces=layout.get("parent_faces", np.arange(len(layout["faces"]))),
        )
        note = {"refined_cells": 0, "edge_midpoints": 0}
    for key in ("precision_mode", "variable_mode", "sampling_mode"):
        if key in layout:
            result[key] = layout[key]
    return result, probe, note


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
    refinement_rounds=1,
):
    if type(refinement_rounds) is not int or refinement_rounds not in (1, 2):
        raise ValueError(
            "one or two explicit bounded terrain refinement rounds required"
        )
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
    layout, probe, refinement = refinement_pass(initial, upper, footprint, sample)
    passes = [{"probe": probe, "refinement": refinement}]
    if refinement_rounds == 2 and refinement["refined_cells"]:
        layout, second_probe, second_refinement = refinement_pass(
            layout, upper, footprint, sample
        )
        passes.append({"probe": second_probe, "refinement": second_refinement})
    if precision != "legacy":
        layout["precision_mode"] = np.array(precision)
    if variables != "heights":
        layout["variable_mode"] = np.array(variables)
    if sampling != "uniform":
        initial["sampling_mode"] = np.array(sampling)
        layout["sampling_mode"] = np.array(sampling)
    note = {
        "initial_partition": note,
        "refinement_probe": probe,
        "refinement": refinement,
        "probe_scope": "Only selects free soil points; not a bridge-clearance-admitted surface.",
    }
    if refinement_rounds != 1:
        note.update(refinement_rounds=refinement_rounds, refinement_passes=passes)
    return (
        initial,
        layout,
        ground,
        upper,
        footprint,
        note,
    )
