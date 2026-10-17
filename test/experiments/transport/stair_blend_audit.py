"""Continuous whole contact and a derived Float32 height envelope, without a solve."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from stair_terrain_audit import body_proof, clearance
from surface_height_checks import normal_gradients


def continuous_cap(body, faces):
    positions = body.vertices
    used = np.unique(faces)
    if len(np.unique(positions[used, :2], axis=0)) != len(used):
        raise ValueError("terrain cap is not a single continuous XY height field")
    polygons = shapely.polygons(positions[faces, :2])
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("terrain cap has projected overlaps or unmatched interfaces")
    shape = shapely.coverage_union_all(polygons)
    if shape.geom_type != "Polygon" or len(shape.interiors):
        raise ValueError("one complete terrain domain without holes required")
    projected = positions[faces, :2]
    u, v = projected[:, 1] - projected[:, 0], projected[:, 2] - projected[:, 0]
    if np.any(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0] <= 0):
        raise ValueError("inverted or collapsed height-field facet")
    return shape


def ground_subdivision(contact, ground, lipschitz):
    source = ground.vertices[ground.triangles]
    polygons = shapely.polygons(source[:, :, :2])
    tree = shapely.STRtree(polygons)
    gradients = normal_gradients(
        ground.vertices[:, :2], ground.triangles, ground.vertices[:, 2]
    )
    sampler = SurfaceSampler(ground)
    guard = sampler.xy_tolerance_m + 64 * np.finfo(float).eps * max(
        1.0, float(abs(ground.vertices).max())
    )
    radius = guard / np.cos(np.pi / 128)
    expanded = shapely.buffer(polygons, radius, quad_segs=32)
    children = [[] for _ in polygons]
    max_height_error = 0.0
    candidates = 0
    for xyz in contact.vertices[contact.triangles]:
        shape = shapely.Polygon(xyz[:, :2])
        matched = False
        for owner in tree.query(shape, predicate="dwithin", distance=guard):
            candidates += 1
            if candidates > 1000000:
                raise ValueError("whole-contact parent candidate cap exhausted")
            if not expanded[owner].covers(shape):
                continue
            plane = (
                source[owner, 0, 2]
                + (xyz[:, :2] - source[owner, 0, :2]) @ gradients[owner]
            )
            error = float(abs(xyz[:, 2] - plane).max())
            if error > sampler.height_tolerance_m:
                continue
            children[owner].append(shape)
            max_height_error = max(max_height_error, error)
            matched = True
        if not matched:
            raise ValueError("ground cap lacks a whole original affine parent")
    for original, parts in zip(polygons, children, strict=True):
        if not parts or not shapely.union_all(parts).buffer(
            radius, quad_segs=32
        ).covers(original):
            raise ValueError(
                "an original ground facet lacks complete metric contact coverage"
            )
    source_slope = float(np.linalg.norm(gradients, axis=1).max())
    return {
        "original_facets": len(polygons),
        "interface_metric_coverage_bound_m": radius,
        "maximum_parent_height_error_m": max_height_error,
        "maximum_source_facet_grade": source_slope,
        "derived_double_contact_bound_m": max_height_error
        + (lipschitz + source_slope) * radius,
    }


def shell(body, parts, domain):
    count = len(parts["top_faces"])
    if not np.array_equal(body.triangles[:count], parts["top_faces"]):
        raise ValueError("saved terrain cap order changed")
    top = body.vertices[parts["top_faces"]]
    bottom = body.vertices[body.triangles[count : 2 * count]][:, ::-1]
    if (
        not np.array_equal(bottom[:, :, :2], top[:, :, :2])
        or np.ptp(bottom[:, :, 2]) != 0
        or bottom[:, :, 2].max() >= top[:, :, 2].min()
    ):
        raise ValueError("terrain lacks its ordered flat lower cap")
    if not np.array_equal(body.triangles[2 * count :], parts["wall_faces"]):
        raise ValueError("saved terrain side-face order changed")
    wall = body.vertices[parts["wall_faces"]]
    projected_normal = np.cross(wall[:, 1] - wall[:, 0], wall[:, 2] - wall[:, 0])[:, 2]
    guard = 64 * np.finfo(float).eps * max(1.0, float(abs(body.vertices).max()))
    if np.any(projected_normal != 0) or not np.all(
        shapely.covered_by(
            shapely.linestrings(wall[:, :, :2]), domain.boundary.buffer(guard)
        )
    ):
        raise ValueError("terrain has nonvertical or interior side faces")


def contact_envelope(terrain, top_faces, ground, reference_error=0.0):
    used = np.unique(top_faces)
    terrain_actual = terrain.vertices.astype(np.float32).astype(float)
    source_actual = ground.vertices.astype(np.float32).astype(float)
    source_used = np.unique(ground.triangles)
    terrain_xy = float(
        np.linalg.norm(
            terrain_actual[used, :2] - terrain.vertices[used, :2], axis=1
        ).max()
    )
    source_xy = float(
        np.linalg.norm(
            source_actual[source_used, :2] - ground.vertices[source_used, :2], axis=1
        ).max()
    )
    terrain_z = float(abs(terrain_actual[used, 2] - terrain.vertices[used, 2]).max())
    source_z = float(
        abs(source_actual[source_used, 2] - ground.vertices[source_used, 2]).max()
    )
    slope = float(
        np.linalg.norm(
            normal_gradients(
                terrain.vertices[:, :2], top_faces, terrain.vertices[:, 2]
            ),
            axis=1,
        ).max()
    )
    # A barycentric point moves by at most the largest corresponding vertex move.
    # The continuous Double field is globally Lipschitz on its convex domain.
    bound = reference_error + terrain_z + source_z + slope * (terrain_xy + source_xy)
    return {
        "terrain_xy_rounding_m": terrain_xy,
        "ground_xy_rounding_m": source_xy,
        "terrain_z_rounding_m": terrain_z,
        "ground_z_rounding_m": source_z,
        "double_height_field_lipschitz": slope,
        "double_reference_contact_error_m": reference_error,
        "derived_vertical_contact_bound_m": bound,
        "formula": "e_double_contact + delta_z_terrain + delta_z_ground + L * (delta_xy_terrain + delta_xy_ground)",
    }


def inspect(body, parts, ground, upper, *, body_max_faces=65536):
    top_faces = parts["top_faces"]
    domain = continuous_cap(body, top_faces)
    if not domain.equals(domain.convex_hull):
        raise ValueError("convex domain required for the global Lipschitz bound")
    if parts["retaining_wall"].any():
        raise ValueError(
            "interior retaining jumps invalidate the continuous contact bound"
        )
    actual = Mesh(body.vertices.astype(np.float32).astype(float), body.triangles)
    actual_domain = continuous_cap(actual, top_faces)
    shell(body, parts, domain)
    shell(actual, parts, actual_domain)
    source_actual = Mesh(
        ground.vertices.astype(np.float32).astype(float), ground.triangles
    )
    upper_actual = Mesh(
        upper.vertices.astype(np.float32).astype(float), upper.triangles
    )
    contact = Mesh(body.vertices, top_faces[parts["ground"]])
    original_shape = shapely.union_all(
        shapely.polygons(ground.vertices[ground.triangles, :2])
    )
    if not domain.covers(original_shape):
        raise ValueError("Double terrain domain does not cover the original ground")
    contact_shape = shapely.union_all(
        shapely.polygons(contact.vertices[contact.triangles, :2])
    )
    if contact_shape.symmetric_difference(original_shape).area > 1e-7:
        raise ValueError(
            "Double terrain lost part of the original whole ground footprint"
        )
    double_contact = [clearance(contact, ground), clearance(ground, contact)]
    if any(abs(row["minimum_clearance_m"]) > 1e-7 for row in double_contact):
        raise ValueError("whole Double underside contact changed")
    source_shape = shapely.union_all(
        shapely.polygons(source_actual.vertices[source_actual.triangles, :2])
    )
    if not actual_domain.covers(source_shape):
        raise ValueError(
            "actual Float32 terrain does not cover the whole ground product"
        )
    cap = Mesh(actual.vertices, top_faces)
    contact_actual = [clearance(cap, source_actual), clearance(source_actual, cap)]
    initial_envelope = contact_envelope(body, top_faces, ground)
    subdivision = ground_subdivision(
        contact, ground, initial_envelope["double_height_field_lipschitz"]
    )
    envelope = contact_envelope(
        body, top_faces, ground, subdivision["derived_double_contact_bound_m"]
    )
    numerical_guard = (
        64 * np.finfo(float).eps * max(1.0, float(abs(actual.vertices).max()))
    )
    if any(
        abs(row["minimum_clearance_m"])
        > envelope["derived_vertical_contact_bound_m"] + numerical_guard
        for row in contact_actual
    ):
        raise ValueError("Float32 contact exceeds its derived representation envelope")
    bridge = {
        "double": clearance(Mesh(body.vertices, top_faces), upper),
        "float32": clearance(cap, upper_actual),
    }
    if any(row["minimum_clearance_m"] < -numerical_guard for row in bridge.values()):
        raise ValueError("actual terrain intrudes into a bridge underside")
    bodies = {
        "double": body_proof(body, max_faces=body_max_faces),
        "float32": body_proof(actual, max_faces=body_max_faces),
    }
    if any(
        p["zero_area_faces"] or p["closed_oriented_volume_m3"] <= 0
        for p in bodies.values()
    ):
        raise ValueError("actual terrain body is collapsed or has nonpositive volume")
    return {
        "bodies": bodies,
        "whole_double_contact": double_contact,
        "whole_float32_contact": contact_actual,
        "float32_contact_envelope": envelope,
        "original_ground_facet_coverage": subdivision,
        "bridge_clearance": bridge,
        "interior_height_jumps": 0,
        "actual_domain_area_m2": actual_domain.area,
        "scope": "Local whole source beds with a bounded Float32 representation; no exact-zero overlap-volume, statutory or global source admission claim.",
    }
