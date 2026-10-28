"""Independent whole mesh topology, original planar faces and finite grades."""

from collections import Counter
from itertools import pairwise

import numpy as np
import shapely


def audit(result, *, expected_holes=2, prefix="two_cycle"):
    if type(expected_holes) is not int or not 2 <= expected_holes <= 8:
        raise ValueError(prefix + "_hole_budget")
    inputs, mesh, geometry = (result[key] for key in ("inputs", "mesh", "geometry"))
    points = mesh.vertices[mesh.triangles]
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    if not np.isfinite(points).all() or np.any(normals[:, 2] <= 0):
        raise ValueError(prefix + "_surface_orientation")
    triangles = shapely.polygons(points[:, :, :2])
    if not shapely.coverage_is_valid(triangles):
        raise ValueError(prefix + "_nonconforming_surface")
    domain = shapely.coverage_union_all(triangles)
    if (
        domain.geom_type != "Polygon"
        or not domain.is_valid
        or len(domain.interiors) != expected_holes
    ):
        raise ValueError(prefix + "_hole_topology")
    axes = np.asarray(geometry["source_nodes_EN"])
    source_edges = np.asarray(geometry["source_edges_local"])
    originals = tuple(shapely.polygonize(shapely.linestrings(axes[source_edges])).geoms)
    if len(originals) != expected_holes:
        raise ValueError(prefix + "_original_plane_faces")
    holes = [shapely.Polygon(ring) for ring in domain.interiors]
    owners = []
    for hole in holes:
        matches = [i for i, face in enumerate(originals) if face.covers(hole)]
        if len(matches) != 1:
            raise ValueError(prefix + "_original_hole_not_preserved")
        owners.extend(matches)
    if len(set(owners)) != expected_holes:
        raise ValueError(prefix + "_original_hole_not_preserved")
    edges = Counter(
        tuple(sorted((int(a), int(b))))
        for face in mesh.triangles
        for a, b in pairwise([*face, face[0]])
    )
    if (
        any(count not in (1, 2) for count in edges.values())
        or len(mesh.vertices) - len(edges) + len(mesh.triangles) != 1 - expected_holes
    ):
        raise ValueError(prefix + "_mesh_euler_or_manifold")
    prior = shapely.coverage_union_all(
        shapely.polygons(inputs.surface.vertices[inputs.surface.triangles, :2])
    )
    if not domain.equals(prior):
        raise ValueError(prefix + "_domain_changed_in_height_fit")
    for edge in inputs.port_edges:
        if not domain.covers(shapely.LineString(inputs.surface.vertices[edge, :2])):
            raise ValueError(prefix + "_whole_section_outside_domain")
    gradients = -normals[:, :2] / normals[:, 2, None]
    frames = (
        result["field"].plan.grade_frames
        if hasattr(result["field"], "plan")
        else inputs.grade_frames
    )
    maximum = float(
        np.linalg.norm(np.einsum("fij,fj->fi", frames, gradients), axis=1).max()
    )
    if maximum > 1.0000001:
        raise ValueError(prefix + "_whole_finite_grade")
    return {
        "holes": expected_holes,
        "hole_areas_m2": sorted(hole.area for hole in holes),
        "original_cycle_face_areas_m2": sorted(face.area for face in originals),
        "domain_area_m2": domain.area,
        "mesh_euler_characteristic": 1 - expected_holes,
        "whole_sections": len(inputs.port_edges),
        "vertices": len(mesh.vertices),
        "triangles": len(mesh.triangles),
        "maximum_normalized_finite_grade": maximum,
        "scope": "Independent original-axis polygonization, distinct source-hole containment, actual coverage/Euler/edge multiplicity, unchanged XY domain, complete sections and finite grades. Existing field checks own full C1/prior qualification; no Source or World authority.",
    }
