"""Whole saved terrain and pier admission, without a mesh builder or field solve."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_blend_audit import inspect
from stair_blend_solve import check_heights
from stair_blend_verify import field_contract
from stair_support_audit import Reference


def source_ground(ground, data):
    old = ground.vertices[ground.triangles]
    new = (data["surface_vertices"] - [0, 0, 0.3])[data["surface_faces"]]
    xyz = np.vstack([old, new])
    return Mesh(xyz.reshape(-1, 3), np.arange(xyz.size // 3).reshape(-1, 3))


def field_link(state, local, local_heights, chosen, world, world_heights):
    old = state["plan"]
    region = shapely.union_all(shapely.polygons(old["xy"][old["faces"][chosen]]))
    pieces = shapely.polygons(local["xy"][local["faces"]])
    if (
        not shapely.coverage_is_valid(pieces)
        or region.symmetric_difference(shapely.union_all(pieces)).area > 1e-7
    ):
        raise ValueError("stored local field changes its bounded old region")
    boundary = shapely.dwithin(region.boundary, shapely.points(local["xy"]), 1e-7)
    old_values = {tuple(p): h for p, h in zip(old["xy"], state["heights"], strict=True)}
    try:
        expected = np.array([old_values[tuple(p)] for p in local["xy"][boundary]])
    except KeyError as error:
        raise ValueError("stored local boundary has a new vertex") from error
    if not np.array_equal(boundary, local["boundary"]) or not np.array_equal(
        expected, local["bed"][boundary]
    ):
        raise ValueError("stored local boundary differs from exact cached heights")
    lookup = {tuple(p): i for i, p in enumerate(world["xy"])}
    if len(lookup) != len(world["xy"]):
        raise ValueError("duplicate whole-world field coordinates")
    try:
        mapped = np.array([lookup[tuple(p)] for p in local["xy"]])
    except KeyError as error:
        raise ValueError("whole world lost a local field vertex") from error
    first = int((~chosen).sum())
    if (
        not np.array_equal(world_heights[mapped], local_heights)
        or not np.array_equal(world["faces"][first:], mapped[local["faces"]])
        or not np.array_equal(world["ground"][first:], local["ground"])
    ):
        raise ValueError("stored local and whole-world field products differ")
    return {
        "linked_local_vertices": len(mapped),
        "linked_local_faces": len(local["faces"]),
        "exact_cached_boundary_vertices": int(boundary.sum()),
    }


def inspect_world(
    state,
    local,
    local_h,
    chosen,
    plan,
    heights,
    body,
    parts,
    ground,
    upper,
    footprint,
    *,
    expected_local_roles,
    body_max_faces=65536,
    surface_max_faces=32768,
):
    if chosen.dtype.kind != "b" or chosen.shape != (len(state["plan"]["faces"]),):
        raise ValueError("whole cached terrain selection required")
    n = int((~chosen).sum())
    before = np.c_[state["plan"]["xy"], state["heights"]][
        state["plan"]["faces"][~chosen]
    ]
    after = np.c_[plan["xy"], heights][plan["faces"][:n]]
    if before.tobytes() != after.tobytes():
        raise ValueError("outside old terrain XYZ changed")
    expected = np.c_[plan["xy"], heights][plan["faces"]]
    if not np.array_equal(
        body.vertices[parts["top_faces"]], expected
    ) or not np.array_equal(parts["ground"], plan["ground"]):
        raise ValueError("saved world body and field differ")
    local_model = field_contract(local, upper, footprint)
    local_roles, local_grades = check_heights(local, local_h, local_model)
    if expected_local_roles.dtype.kind != "b" or not np.array_equal(
        local_roles, expected_local_roles
    ):
        raise ValueError("saved local constructed-soil classes changed")
    global_model = field_contract(plan, upper, footprint)
    roles, grades = check_heights(plan, heights, global_model)
    physical = inspect(
        body,
        parts,
        ground,
        upper,
        body_max_faces=body_max_faces,
        surface_max_faces=surface_max_faces,
    )
    floor = Reference(Mesh(body.vertices, parts["top_faces"]))
    contacts = [floor.check(low) for low, _high in state["pier_contacts"]]
    return roles, {
        "physical": physical,
        "grades": grades,
        "local_grades": local_grades,
        "unchanged_outside_triangles": n,
        "old_pier_contacts": len(contacts),
        "maximum_pier_contact_error_float32_m": max(
            (q["float32_contact_error_m"] for q in contacts), default=0
        ),
    }
