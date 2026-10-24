"""One solved height plan drives both owned bodies, navigation and terrain hints."""

import time

import numpy as np
from compare_mvt_native_body import orientation_counts
from mvt_independent_flight_inputs import prepared
from mvt_native_body_products import audit_rendered_contacts, consumed_plan
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_products import shared_products
from mvt_t_profile_solve import solve_profiles
from outshine_ref.contracts import Mesh
from stair_junction_body import build
from stair_network_geometry import vertex_links
from surface_solid_checks import audit_closed


def construct(
    arrangement="stacked",
    *,
    solver="highs-ds",
    presolve=False,
    global_pose=None,
    cut_scope="flight_footprint",
    cut_stage="source_heights",
):
    began = time.perf_counter()
    solver_plan, contacts = prepared(
        arrangement, global_pose=global_pose, cut_scope=cut_scope, cut_stage=cut_stage
    )
    prepared_at = time.perf_counter()
    solver_heights, solve = solve_profiles(
        solver_plan,
        fixed_contacts=contacts,
        max_prior_change_m=3,
        gradient_model="axis_crossfall",
        solver=solver,
        contact_presolve=presolve,
    )
    if solver_heights is None:
        raise ValueError(solve)
    solved_at = time.perf_counter()
    body, data, geometry = build(solver_plan, solver_heights)
    plan, heights = consumed_plan(solver_plan, data)
    built_at = time.perf_counter()
    proof = audit_profiles(
        plan, heights, data, max_prior_change_m=3, gradient_model="axis_crossfall"
    )
    fixed = audit_rendered_contacts(plan, data, contacts)
    links = vertex_links(body.vertices, body.triangles)
    orientation = orientation_counts(data)
    if orientation["invalid_float32_faces"]:
        raise ValueError("independent body contains invalid Float32 triangles")
    upper_ids = set(plan["controlled_derivation"]["upper_source_mapping"])
    upper_mask = sum(
        1 << i for i, row in enumerate(plan["rows"]) if row["source_id"] in upper_ids
    )
    is_upper = (data["face_sources"] & upper_mask) != 0
    xyz = data["body_vertices"].astype(np.float32)
    low_high = float(xyz[data["top_faces"][~is_upper], 2].max())
    high_low = float(xyz[data["bottom_faces"][is_upper], 2].min())
    split_height = (low_high + high_low) / 2
    if high_low <= low_high:
        raise ValueError(
            "this controlled two-network example lacks vertical body separation"
        )
    face_height = xyz[data["body_faces"], 2]
    lower, upper = (
        (face_height < split_height).all(axis=1),
        (face_height > split_height).all(axis=1),
    )
    if not (lower | upper).all():
        raise ValueError("body face incorrectly joins the independent height bands")
    volumes = [
        audit_closed(Mesh(xyz, data["body_faces"][selected]))
        for selected in (lower, upper)
    ]
    if min(volumes) <= 0:
        raise ValueError("each independent Float32 body needs positive closed volume")
    audited_at = time.perf_counter()
    products = shared_products(plan, heights, data)
    finished_at = time.perf_counter()
    return {
        "solver_plan": solver_plan,
        "solver_heights": solver_heights,
        "plan": plan,
        "contacts": contacts,
        "heights": heights,
        "body": body,
        "data": data,
        "geometry": geometry,
        "proof": proof,
        "fixed_profiles": fixed,
        "vertex_links": links,
        "float32_orientation": orientation,
        "float32_body_volumes_m3": volumes,
        "global_vertical_clearance_bound_m": high_low - low_high,
        "products": products,
        "solve": solve,
        "seconds": {
            "prepare": prepared_at - began,
            "fit": solved_at - prepared_at,
            "body": built_at - solved_at,
            "audit": audited_at - built_at,
            "products": finished_at - audited_at,
            "total": finished_at - began,
        },
    }
