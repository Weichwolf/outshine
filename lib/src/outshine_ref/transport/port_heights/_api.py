"""Bounded public shared-port planning with independently checked finite products."""

import time
from dataclasses import replace

import numpy as np
import shapely

from ...contracts import Mesh
from ._budget import DEFAULT_BUDGET, PortHeightBudget, PortLimit
from ._contacts import contact_audit
from ._input import PortHeightInput, prepare_input
from ._layout import prepare_plane_field
from ._plan import PortHeightFit, PortHeightPlan
from ._solve import solve_slices


def input_domain(source):
    faces = source.surface.triangles
    edges = faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    _, labels, count = np.unique(
        np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    winding = np.bincount(labels, weights=np.where(edges[:, 0] < edges[:, 1], 1, -1))
    if np.any(count > 2) or np.any(winding[count == 2] != 0):
        raise ValueError("conforming consistently oriented source edges required")
    polygons = shapely.polygons(source.surface.vertices[faces, :2])
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("nonoverlapping edge-matched source coverage required")
    return shapely.coverage_union_all(polygons)


def final_plan(model, product, captured, raw, domain):
    mesh, coefficients, seed, _marked, errors, records = raw
    audit = records[-1]["quality"]
    fitted = any(r["fitting"] and r["fitting"]["solver"]["valid"] for r in records)
    if not fitted or not audit["geometry_complete"] or not audit["whole_world_valid"]:
        return None, {
            "valid": False,
            "reason": "port_fit_or_geometry_budget",
            "audit": audit,
        }
    polygons = shapely.polygons(mesh["xy"][mesh["faces"]])
    if not shapely.coverage_is_valid(polygons):
        return None, {"valid": False, "reason": "port_finite_coverage_unresolved"}
    coverage_error = shapely.symmetric_difference(
        shapely.coverage_union_all(polygons), domain
    ).area
    if coverage_error > 1e-6:
        return None, {
            "valid": False,
            "reason": "port_finite_coverage_unresolved",
            "coverage_error_m2": coverage_error,
        }
    planes = coefficients[: 3 * len(model.origins)]
    contacts = contact_audit(
        model,
        planes,
        product,
        mesh["heights"][mesh["original_vertex_map"][captured["road_vertex_map"]]],
    )
    port_edges = np.asarray(list(mesh["port_roots"]), dtype=np.int64).reshape(-1, 2)
    plan = PortHeightPlan(
        field=model,
        mesh=Mesh(np.c_[mesh["xy"], mesh["heights"]], mesh["faces"]),
        plane_coefficients=planes,
        datum_m=float(seed["datum_m"]),
        face_parts=mesh["parts"],
        grade_frames=mesh["frames"],
        source_faces=mesh["roots"],
        original_vertex_map=mesh["original_vertex_map"],
        port_edges=port_edges,
        port_ids=np.asarray(list(mesh["port_roots"].values()), dtype=np.int64),
        height_error_bounds_m=errors,
        targets_m=mesh["targets"],
        parameter_intervals=mesh["intervals"],
    )
    return plan, {
        "valid": True,
        "reason": "",
        "audit": audit,
        "analytic_contacts": contacts,
        "coverage_error_m2": coverage_error,
        "maximum_original_prior_error_m": float(
            abs(
                plan.mesh.vertices[plan.original_vertex_map, 2]
                - captured["ground_targets"]
            ).max()
        ),
    }


def plan_port_heights(source, *, budget=DEFAULT_BUDGET, method="cone", progress=None):
    """Plan complete local C1 road patches and a conforming C0 road/soil surface.

    Source vertices hold soft height priors; only original observations enter the
    fit. Nominal whole gradient bound is 1+1e-7, analytic height chords <=0.01m,
    finite whole-port gradient jumps <=0.005. Failures publish no unchecked plan;
    report.part_usage gives a reason for every original module. Original sources,
    metric frame and kind remain caller-bound through unchanged module/face IDs.
    Clarabel is optional and loaded only for fitting. This is offline work with
    cooperative time limits; hard process memory/CPU limits belong to the worker.
    A valid MaxTime candidate proves feasibility, not optimality or driving/LOD.
    """
    began = time.monotonic()
    if not isinstance(source, PortHeightInput) or not isinstance(
        budget, PortHeightBudget
    ):
        raise TypeError("owned PortHeightInput and PortHeightBudget required")
    if method not in ("cone", "lp") or (
        progress is not None and not callable(progress)
    ):
        raise ValueError("known solver and optional callable progress required")
    report = {
        "valid": False,
        "reason": "port_input_budget",
        "rounds": [],
        "method": method,
    }
    sizes = (
        len(source.surface.vertices),
        len(source.surface.triangles),
        len(source.part_roles),
        len(source.port_edges),
    )
    caps = (budget.max_vertices, budget.max_faces, budget.max_parts, budget.max_ports)
    plan = None
    if all(n <= cap for n, cap in zip(sizes, caps)):
        if budget.time_limit_s == 0 or budget.solver_time_limit_s == 0:
            report["reason"] = "port_time_budget"
        else:
            domain = input_domain(source)
            product, captured, original = prepare_input(source)
            model, layout = prepare_plane_field(product)
            model = replace(model, max_queries=budget.max_queries)
            report.update(
                layout=layout, preparation_ms=(time.monotonic() - began) * 1000
            )
            datum = float(np.median(captured["ground_targets"]))
            seed = {
                "plane_coefficients": np.zeros(3 * len(model.origins)),
                "vertices": np.c_[original["xy"], np.full(len(original["xy"]), datum)],
                "datum_m": datum,
            }
            try:
                raw = solve_slices(
                    model,
                    product,
                    captured,
                    original,
                    seed,
                    budget=budget,
                    method=method,
                    progress=progress,
                    records=report["rounds"],
                    began=began,
                )
                plan, checked = final_plan(model, product, captured, raw, domain)
                report.update(checked)
            except PortLimit as error:
                report["reason"] = str(error)
            except MemoryError:
                report["reason"] = "port_address_space_budget"
            except ImportError:
                report["reason"] = "port_solver_dependency_missing"
            except (FloatingPointError, ValueError) as error:
                report.update(
                    reason="port_numeric_geometry_unresolved", detail=str(error)
                )
    report.update(
        total_ms=(time.monotonic() - began) * 1000,
        requested_time_limit_s=budget.time_limit_s,
        time_overrun_s=max(0, time.monotonic() - began - budget.time_limit_s),
        part_usage=[
            {
                "part": i,
                "status": "used" if plan else "not_used",
                "reason": "" if plan else report["reason"],
            }
            for i in range(len(source.part_roles))
        ],
        scope="C1 analytic road field, C0 finite road/soil mesh; no complete source/world/dynamics/LOD/native proof.",
    )
    return PortHeightFit(plan, report)
