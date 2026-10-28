"""Explicit fitted boundary option; original road and four DEM corners stay fixed."""

from dataclasses import asdict

import numpy as np
import shapely
from ground_contact import (
    ContactLimits,
    contact_input,
    corner_witness,
    finish_contact,
    maximum_grade,
)
from ground_contact_profiles import WholeProfileGuard, exact_squared_excess
from ground_rim_fit import fit_rim_heights
from ground_rim_sampling import RimRecipe, observe_rim, sample_values
from ground_surface import ground_layout
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import HeightBudget


def rim_candidate(road, prior, sample_z, margin, limits, recipe, method, budget, guard):
    rectangle = shapely.box(
        *(road.vertices[:, :2].min(axis=0) - margin),
        *(road.vertices[:, :2].max(axis=0) + margin),
    )
    corners = np.asarray(rectangle.exterior.coords)[:-1]
    heights = sample_values(sample_z, corners)
    witness = corner_witness(road, corners, heights, limits.soil_grade * 1.0000001)
    record = {"margin_m": margin, "corner_height_witness": witness}
    if witness["excess_m"] > 0:
        a = road.vertices[witness["fixed_vertex"]]
        b = np.r_[corners[witness["corner"]], heights[witness["corner"]]]
        excess = exact_squared_excess(
            np.array([a, a]), np.array([b, b]), 0.0, 0.0, guard.grade
        )
        if excess > 0 and guard.road_condition():
            return None, record | {
                "status": "pending",
                "reason": "fixed_corner_lipschitz_conflict",
                "exact_squared_excess_m2": str(excess),
            }
    # Intermediate DEM values are observations, not fixed chord constraints.
    # The old whole-chord rejection is therefore deliberately inapplicable here.
    xy, z, sampling = observe_rim(corners, heights, sample_z, recipe)
    domain = shapely.Polygon(xy)
    layout, work = ground_layout(
        road.vertices[:, :2],
        road.triangles,
        domain,
        method="coverage",
        max_faces=limits.maximum_faces,
    )
    mapping = layout["road_vertex_map"]
    outer = np.setdiff1d(np.arange(len(layout["xy"])), mapping)
    lookup = {tuple(p): float(v) for p, v in zip(xy, z, strict=True)}
    if len(outer) != len(xy) or any(
        tuple(layout["xy"][i]) not in lookup for i in outer
    ):
        raise ValueError("unexpected_unbound_rim_vertex")
    vertices = np.empty((len(layout["xy"]), 3))
    vertices[:, :2] = layout["xy"]
    vertices[mapping] = road.vertices
    for at in outer:
        vertices[at, 2] = lookup[tuple(vertices[at, :2])]
    byxy = {tuple(p): i for i, p in enumerate(vertices[:, :2])}
    corner_ids = np.array([byxy[tuple(p)] for p in corners])
    fixed = {int(i): float(vertices[i, 2]) for i in [*mapping, *corner_ids]}
    frames = np.broadcast_to(
        np.eye(2) / limits.soil_grade, (len(layout["faces"]), 2, 2)
    ).copy()
    # Source road profiles are already fixed and retain their own producer's
    # constraints; the soil limit does not redefine their permitted gradients.
    road_grade = maximum_grade(road.vertices, road.triangles)
    frames[: len(road.triangles)] = np.eye(2) / max(
        limits.soil_grade, road_grade * 1.000001
    )
    ordered = np.array([byxy[tuple(p)] for p in xy])
    lengths = np.linalg.norm(np.roll(xy, -1, axis=0) - xy, axis=1)
    weights = np.ones(len(vertices))
    weights[ordered] = (lengths + np.roll(lengths, 1)) / (2 * lengths.sum())
    fit = fit_rim_heights(
        Mesh(vertices, layout["faces"]),
        frames,
        fixed=fixed,
        budget=budget,
        maximum_change_m=recipe.maximum_adjustment_m,
        weights=weights,
        eliminate_fixed=method == "reduced",
    )
    record.update(sampling=sampling, work=work, fit=dict(fit.report))
    if fit.mesh is None:
        return None, record | {
            "status": "pending",
            "reason": "rim_fit_without_valid_surface",
        }
    fitted = fit.mesh.vertices.copy()
    for at, height in fixed.items():
        fitted[at, 2] = height
    grade = maximum_grade(fitted, layout["faces"][len(road.triangles) :])
    adjustment = float(abs(fitted[outer, 2] - vertices[outer, 2]).max())
    record.update(maximum_soil_grade=grade, maximum_observation_adjustment_m=adjustment)
    if (
        grade > limits.soil_grade * 1.0000001
        or adjustment > recipe.maximum_adjustment_m
    ):
        return None, record | {"status": "pending", "reason": "rim_fit_quality_limit"}
    observations = np.empty(len(vertices))
    observations[mapping] = prior
    observations[outer] = vertices[outer, 2]
    return {
        "mesh": Mesh(fitted, layout["faces"]),
        "road_vertex_map": mapping,
        "domain": domain,
        "outer_vertex_ids": outer,
        "prior_z": observations,
        "rim_profile": {
            "recipe": asdict(recipe),
            "fit_method": method,
            "corner_vertex_ids": corner_ids,
            "ordered_vertex_ids": np.array([byxy[tuple(p)] for p in xy]),
            "sampling": sampling,
            "fit": dict(fit.report),
            "maximum_observation_adjustment_m": adjustment,
        },
    }, record | {"status": "candidate", "reason": ""}


def build_rim_contact(
    road,
    road_dem_z,
    sample_z,
    *,
    limits=None,
    recipe=None,
    fit_method="reduced",
    budget=None,
):
    limits = ContactLimits() if limits is None else limits
    recipe = RimRecipe() if recipe is None else recipe
    budget = (
        HeightBudget(time_limit_s=0.25, max_vertices=1024, max_faces=4096)
        if budget is None
        else budget
    )
    if fit_method not in ("full", "reduced"):
        raise ValueError("Known rim fit method required")
    prior, allowed = contact_input(road, road_dem_z, limits)
    mandatory = 4 * recipe.segments_per_side if recipe.sampling == "uniform" else 4
    if (
        not allowed
        or budget.time_limit_s == 0
        or len(road.vertices) + mandatory > budget.max_vertices
        or len(road.triangles) > budget.max_faces
    ):
        return {
            "status": "pending",
            "reason": "rim_input_or_time_budget",
            "candidates": [],
        }
    guard = WholeProfileGuard(road, limits.soil_grade * 1.0000001)
    candidates = []
    chosen = None
    for margin in limits.margins_m:
        try:
            value, record = rim_candidate(
                road, prior, sample_z, margin, limits, recipe, fit_method, budget, guard
            )
        except ValueError as error:
            value, record = (
                None,
                {"margin_m": margin, "status": "pending", "reason": str(error)},
            )
        candidates.append(record)
        if value is not None:
            chosen = value
            break
    if chosen is None:
        return {
            "status": "pending",
            "reason": "no_qualified_rim_contact_variant",
            "candidates": candidates,
        }
    result = finish_contact(
        road,
        chosen,
        candidates,
        sample_z,
        limits,
        maximum_grade(road.vertices, road.triangles),
    )
    result["scope"] = (
        "Exact immutable source road/four outer DEM corners; explicitly fitted intermediate boundary observations under whole soil gradients, finite local C0 mesh/refinement only. No native original-grid/neighbour/World/Source/device admission."
    )
    return result
