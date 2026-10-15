"""Preserve feasible global planes at underdetermined free road ends."""

import time

import numpy as np

from ._budget import PortLimit
from ._quality import quality
from ._slices import slice_mesh
from ._topology import topology_audit


def affine_heights(model, mesh, planes, gradient, datum, budget):
    road = mesh["parts"] >= 0
    keys = np.unique(
        np.c_[mesh["faces"][road].ravel(), np.repeat(mesh["parts"][road], 3)], axis=0
    )
    vertices, parts = keys.T
    soil = np.setdiff1d(np.arange(len(mesh["xy"])), np.unique(vertices))
    if (
        len(keys) > budget.max_queries
        or 3 * len(model.origins) + len(soil) > budget.max_variables
    ):
        raise PortLimit("port_variable_or_sample_budget")
    heights, gradients = model.evaluate(mesh["xy"][vertices], parts, planes)
    mesh["heights"] = mesh["targets"]
    differences = abs(heights + datum - mesh["heights"][vertices])
    grade_error = float(np.linalg.norm(gradients - gradient, axis=1).max())
    if differences.max() > 0.5e-7 or grade_error > 0.5e-10:
        return None
    delta = np.zeros(len(mesh["xy"]))
    np.maximum.at(delta, vertices, differences)
    coefficients = np.r_[planes, mesh["heights"][soil] - datum]
    return coefficients, max(float(differences.max()), grade_error), delta


def affine_prior(model, product, captured, original, budget, began):
    remaining = budget.time_limit_s - (time.monotonic() - began)
    if remaining <= 0:
        raise PortLimit("port_time_budget")
    xy, z = original["xy"], original["targets"]
    centre, datum = xy.mean(axis=0), float(np.median(z))
    design = np.c_[np.ones(len(xy)), xy - centre]
    line, _residual, rank, _singular = np.linalg.lstsq(design, z - datum, rcond=None)
    if rank != 3:
        return None
    residual = float(abs(design @ line + datum - z).max())
    guard = 64 * np.finfo(float).eps * max(1, abs(z).max())
    if residual > guard:
        return None
    planes = np.c_[
        line[0] + (model.origins - centre) @ line[1:],
        np.tile(line[1:], (len(model.origins), 1)),
    ].ravel()
    seed = {
        "plane_coefficients": planes,
        "vertices": np.c_[xy, z],
        "datum_m": datum,
    }
    mesh, note = slice_mesh(
        model,
        original,
        product,
        captured,
        {},
        seconds=min(budget.geometry_time_limit_s, remaining),
        max_vertices=budget.max_output_vertices,
        max_faces=budget.max_output_faces,
        max_weld_pairs=budget.max_weld_pairs,
    )
    note["topology"] = topology_audit(original, mesh)
    direct = affine_heights(model, mesh, planes, line[1:], datum, budget)
    if direct is None:
        return None
    coefficients, equality_error, delta = direct
    prior_error = float(abs(mesh["heights"][mesh["original_vertex_map"]] - z).max())
    marked, audit, errors = quality(model, planes, mesh, product["part_roles"])
    road = mesh["parts"] >= 0
    errors[road] += delta[mesh["faces"][road]].max(axis=1)
    audit["maximum_height_chord_bound_m"] = float(errors.max())
    if (
        prior_error > guard
        or errors.max() > 0.01
        or not audit["geometry_complete"]
        or not audit["whole_world_valid"]
    ):
        return None
    record = {
        "iteration": 0,
        "mesh": note,
        "quality": audit,
        "equality_error": equality_error,
        "elapsed_ms": (time.monotonic() - began) * 1000,
        "fitting": {
            "system": {
                "objective_observations": len(z),
                "direct_global_plane": True,
                "sparse_nonzeros": 0,
            },
            "solver": {
                "valid": True,
                "reason": "",
                "method": "direct",
                "status": "affine_prior",
                "iterations": 0,
                "global_plane_residual_m": residual,
                "maximum_original_prior_error_m": prior_error,
            },
        },
    }
    return mesh, coefficients, seed, marked, errors, [record]
