"""Whole local source-bed and pier contacts without constructing a global body."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_blend_audit import ground_subdivision
from stair_blend_solve import check_heights
from stair_blend_verify import field_contract
from stair_local_context import radius, require
from stair_module_world import source_ground
from stair_support_audit import Reference
from stair_terrain_audit import clearance
from surface_height_checks import normal_gradients


def frozen_beds(view, state, field):
    road = state["road"]
    faces = road["bottom_faces"][state["road_ground"]]
    triangles = road["body_vertices"][faces]
    region = shapely.union_all(shapely.polygons(field.vertices[field.triangles, :2]))
    shapes = shapely.polygons(triangles[:, :, :2])
    selected = shapely.dwithin(shapes, region, radius(view))
    triangles = triangles[selected]
    footprint = shapely.union_all(shapes[selected])
    local = require(view.nearby(footprint, radius(view), max_faces=8192))
    floor = Reference(local.mesh)
    proofs = [floor.check(Mesh(tri, np.array([[0, 1, 2]]))) for tri in triangles]
    return {
        "whole_original_body_facets": len(proofs),
        "reference_faces": len(local.mesh.triangles),
        "maximum_contact_float32_m": max(p["float32_contact_error_m"] for p in proofs),
        "pair_tests": sum(p["contact_pair_checks"] for p in proofs),
    }


def validate(view, state, local, heights, roles, chosen, data, upper):
    field = Mesh(np.c_[local["xy"], heights], local["faces"])
    old = state["plan"]
    retained = np.c_[old["xy"], state["heights"]][old["faces"][chosen & old["ground"]]]
    new = (data["surface_vertices"] - [0, 0, 0.3])[data["surface_faces"]]
    triangles = np.vstack([retained, new])
    source = Mesh(
        triangles.reshape(-1, 3), np.arange(len(triangles) * 3).reshape(-1, 3)
    )
    footprint = shapely.union_all(shapely.polygons(triangles[:, :, :2]))
    context_ground = source_ground(
        Mesh(np.c_[old["xy"], state["heights"]], old["faces"][old["ground"]]), data
    )
    context_footprint = shapely.union_all(
        shapely.polygons(context_ground.vertices[context_ground.triangles, :2])
    )
    actual_roles, grades = check_heights(
        local, heights, field_contract(local, upper, context_footprint)
    )
    if not np.array_equal(actual_roles, roles):
        raise ValueError("local constructed terrain roles changed")
    slopes = np.linalg.norm(
        normal_gradients(local["xy"], local["faces"], heights), axis=1
    )
    subdivision = ground_subdivision(
        Mesh(field.vertices, local["faces"][local["ground"]]),
        source,
        float(slopes.max()),
    )
    nearby = require(view.nearby(footprint, radius(view), max_faces=8192))
    reference = Reference(nearby.mesh)
    contacts = [reference.check(Mesh(tri, np.array([[0, 1, 2]]))) for tri in triangles]
    bridges = []
    for cast in (False, True):
        terrain = Mesh(
            field.vertices.astype("f4") if cast else field.vertices, field.triangles
        )
        ceiling = Mesh(
            upper.vertices.astype("f4") if cast else upper.vertices, upper.triangles
        )
        proof = clearance(terrain, ceiling)
        guard = 64 * np.finfo(float).eps * max(1.0, float(abs(terrain.vertices).max()))
        if proof["minimum_clearance_m"] < -guard:
            raise ValueError("local actual terrain intrudes into a bridge underside")
        bridges.append(proof)
    return {
        "grades": grades,
        "original_ground_facet_coverage_double": subdivision,
        "source_facet_contacts": len(contacts),
        "maximum_ground_contact_float32_m": max(
            p["float32_contact_error_m"] for p in contacts
        ),
        "ground_contact_pair_tests": sum(p["contact_pair_checks"] for p in contacts),
        "ground_reference_faces": len(nearby.mesh.triangles),
        "frozen_body_beds": frozen_beds(view, state, field),
        "bridge_clearance": bridges,
        "scope": "Whole convex source-facet references and exact patch boundary gluing; no global nonconvex Lipschitz shortcut or new structural claim.",
    }


def piers(view, contacts):
    proofs, maximum = [], 0
    for low, _high in contacts:
        footprint = shapely.union_all(shapely.polygons(low.vertices[low.triangles, :2]))
        selected = require(view.nearby(footprint, radius(view), max_faces=2048))
        proofs.append(Reference(selected.mesh).check(low))
        maximum = max(maximum, len(selected.mesh.triangles))
    return {
        "whole_contacts": len(proofs),
        "maximum_reference_faces": maximum,
        "maximum_contact_float32_m": max(p["float32_contact_error_m"] for p in proofs),
        "pair_tests": sum(p["contact_pair_checks"] for p in proofs),
    }
