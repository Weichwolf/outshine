"""Independent original-band, bed/soil and fixed-profile geometry checks."""

from collections import Counter
from itertools import pairwise

import numpy as np
import shapely
from ground_contact_checks import qualify_contact
from outshine_ref.transport.canonical_ports._geodesy import positions_in_frame


def mesh_domain(mesh, *, expected_holes):
    points = mesh.vertices[mesh.triangles]
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    if not np.isfinite(points).all() or np.any(normals[:, 2] <= 0):
        raise ValueError("rail_audit_invalid_finite_faces")
    polygons = shapely.polygons(points[:, :, :2])
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("rail_audit_nonconforming_or_overlapping_mesh")
    domain = shapely.coverage_union_all(polygons)
    if domain.geom_type != "Polygon" or len(domain.interiors) != expected_holes:
        raise ValueError("rail_audit_surface_holes")
    edges = Counter(
        tuple(sorted((int(a), int(b))))
        for face in mesh.triangles
        for a, b in pairwise([*face, face[0]])
    )
    euler = len(mesh.vertices) - len(edges) + len(mesh.triangles)
    if any(c not in (1, 2) for c in edges.values()) or euler != 1 - expected_holes:
        raise ValueError("rail_audit_surface_euler_or_manifold")
    gradient = np.linalg.norm(-normals[:, :2] / normals[:, 2, None], axis=1)
    return domain, gradient, euler


def original_domain(index, source, anchor):
    nodes, spans, _ = index
    xy = positions_in_frame(
        anchor, [nodes[n] for n in source.node_ids], np.zeros(len(source.node_ids))
    )[:, :2]
    positions = dict(zip(source.node_ids, xy, strict=True))
    bands, caps, ends = [], {}, []
    for member in source.members:
        if (
            member is not spans[member.identifier]
            or member.maximum_gradient is not None
            or member.attributes["recipe"]["native_maximum_gradient_word"] != 0
        ):
            raise ValueError("rail_audit_original_member_or_profile_changed")
        a, b = (positions[n] for n in member.nodes)
        band = shapely.LineString([a, b]).buffer(member.width_m / 2, cap_style="flat")
        bands.append(band)
        corners = np.asarray(band.exterior.coords[:-1])
        for node, point in zip(member.nodes, (a, b), strict=True):
            cap = corners[np.argsort(np.linalg.norm(corners - point, axis=1))[:2]]
            caps.setdefault(node, []).extend(cap)
            ends.append(shapely.LineString(cap))
    joins = []
    for points in caps.values():
        hull = shapely.MultiPoint(points).convex_hull
        if hull.geom_type == "Polygon":
            joins.append(hull)
    return shapely.union_all([*bands, *joins]), bands, ends


def audit(result, index, profile):
    profile.check_index(index)
    source, bed, contact = result["source"], result["bed"], result["terrain"]
    if tuple(m.identifier for m in source.members) != tuple(
        m.identifier for m in profile.members
    ):
        raise ValueError("rail_audit_original_members_lost")
    bindings = {
        identity: (band, direction) for identity, band, direction in source.bindings
    }
    if len(bindings) != len(source.members) or any(
        m.identifier not in bindings
        or not 0 <= bindings[m.identifier][0] < len(source.bands)
        or bindings[m.identifier][1] not in (-1, 1)
        or tuple(source.bands[bindings[m.identifier][0]][:: bindings[m.identifier][1]])
        != m.nodes
        for m in source.members
    ):
        raise ValueError("rail_audit_original_direction_lost")
    expected, bands, profiles = original_domain(index, source, result["anchor"])
    bed_domain, bed_grade, euler = mesh_domain(
        bed, expected_holes=len(expected.interiors)
    )
    if bed_domain.symmetric_difference(expected).area > 1e-8:
        raise ValueError("rail_audit_original_band_union_changed")
    if any(b.difference(bed_domain).area > 1e-8 for b in bands) or any(
        not bed_domain.buffer(1e-8).covers(line) for line in profiles
    ):
        raise ValueError("rail_audit_original_width_or_end_profile_lost")
    if (
        bed_grade.max() > profile.maximum_gradient * 1.0000001
        or abs(result["plane"].evaluate(bed.vertices[:, :2]) - bed.vertices[:, 2]).max()
        > 1e-9
    ):
        raise ValueError("rail_audit_whole_affine_bed_grade")
    mapping = contact["road_vertex_map"]
    complete = contact["mesh"]
    if not np.array_equal(
        complete.vertices[mapping], bed.vertices
    ) or not np.array_equal(
        complete.triangles[: len(bed.triangles)], mapping[bed.triangles]
    ):
        raise ValueError("rail_audit_terrain_changed_fixed_bed")
    terrain_domain, all_grades, _ = mesh_domain(complete, expected_holes=0)
    if terrain_domain.symmetric_difference(contact["domain"]).area > 1e-8:
        raise ValueError("rail_audit_terrain_domain_changed")
    soil_grades = all_grades[len(bed.triangles) :]
    if soil_grades.max() > contact["limits"].soil_grade * 1.0000001:
        raise ValueError("rail_audit_whole_soil_grade")
    outer = contact["outer_vertex_ids"]
    if not np.array_equal(complete.vertices[outer, 2], contact["prior_z"][outer]):
        raise ValueError("rail_audit_fixed_outer_dem_changed")
    contact_proof = qualify_contact(bed, contact)
    return {
        "original_members": len(source.members),
        "original_end_profiles": len(profiles),
        "physical_bands": len(source.bands),
        "bed_holes": len(expected.interiors),
        "bed_euler": euler,
        "bed_area_m2": bed_domain.area,
        "bed_vertices": len(bed.vertices),
        "bed_triangles": len(bed.triangles),
        "terrain_vertices": len(complete.vertices),
        "terrain_triangles": len(complete.triangles),
        "maximum_finite_bed_grade": float(bed_grade.max()),
        "maximum_finite_soil_grade": float(soil_grades.max()),
        "whole_terrain_contact": contact_proof,
        "scope": "Original object/word/direction and full physical-band/end-profile coverage; independent flat line buffers, Euler/edge multiplicity, conforming coverage, constant affine bed and unchanged bed under soil construction. No logical routing, original end/Source/World or continuous DEM-error admission.",
    }
