"""Independent source-ring, mesh topology and complete internal-section controls."""

from collections import Counter
from itertools import pairwise

import numpy as np
import shapely


def audit(result):
    inputs, mesh, geometry = (result[k] for k in ("inputs", "mesh", "geometry"))
    xyz = mesh.vertices[mesh.triangles]
    normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    if not np.isfinite(xyz).all() or np.any(normal[:, 2] <= 0):
        raise ValueError("cycle_surface_orientation")
    polygons = shapely.polygons(xyz[:, :, :2])
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("cycle_nonconforming_surface")
    domain = shapely.coverage_union_all(polygons)
    if (
        domain.geom_type != "Polygon"
        or not domain.is_valid
        or len(domain.interiors) != 1
    ):
        raise ValueError("cycle_hole_topology")
    original = shapely.Polygon(
        np.array(geometry["source_nodes_EN"])[
            list(geometry["original_cycle_local_nodes"])
        ]
    )
    hole = shapely.Polygon(domain.interiors[0])
    if not original.is_valid or original.area <= 0 or not original.covers(hole):
        raise ValueError("cycle_original_ring_not_preserved")
    edges = Counter(
        tuple(sorted((int(a), int(b))))
        for face in mesh.triangles
        for a, b in pairwise([*face, face[0]])
    )
    if (
        any(n not in (1, 2) for n in edges.values())
        or len(mesh.vertices) - len(edges) + len(mesh.triangles) != 0
    ):
        raise ValueError("cycle_mesh_euler_or_manifold")
    gradients = -normal[:, :2] / normal[:, 2, None]
    normalized = np.linalg.norm(
        np.einsum(
            "fij,fj->fi",
            inputs.grade_frames
            if not hasattr(result["field"], "plan")
            else result["field"].plan.grade_frames,
            gradients,
        ),
        axis=1,
    )
    if normalized.max() > 1.0000001:
        raise ValueError("cycle_whole_finite_grade")
    for edge in inputs.port_edges:
        section = shapely.LineString(inputs.surface.vertices[edge, :2])
        # Shared whole sections are actual edges on both adjoining part domains.
        if not domain.covers(section):
            raise ValueError("cycle_whole_section_outside_domain")
    prior = shapely.coverage_union_all(
        shapely.polygons(inputs.surface.vertices[inputs.surface.triangles, :2])
    )
    if not domain.equals(prior):
        raise ValueError("cycle_domain_changed_in_height_fit")
    return {
        "holes": 1,
        "hole_area_m2": hole.area,
        "original_cycle_area_m2": original.area,
        "domain_area_m2": domain.area,
        "mesh_euler_characteristic": 0,
        "whole_sections": len(inputs.port_edges),
        "vertices": len(mesh.vertices),
        "triangles": len(mesh.triangles),
        "maximum_normalized_finite_grade": float(normalized.max()),
        "scope": "Independent actual triangle coverage, source-ring hole containment, Euler/edge multiplicity, unchanged XY domain and complete section inclusion. Existing field-quality checks own C1/whole finite port gradients. No Source or World authority.",
    }
