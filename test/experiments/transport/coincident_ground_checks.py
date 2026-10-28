"""Independent original-band coverage, source orientation and finite surface audit."""

from collections import Counter
from itertools import pairwise

import numpy as np
import shapely


def audit(result, index, selected):
    source, mesh, domain = (result[k] for k in ("source", "mesh", "domain"))
    selected = tuple(sorted(selected))
    if tuple(s.identifier for s in source.members) != selected:
        raise ValueError("cover_lost_original_member")
    nodes, spans, _ = index
    local = {node: xy for node, xy in zip(source.node_ids, result["axes"], strict=True)}
    missing_areas, sections = [], []
    bindings = {
        identity: (band, direction) for identity, band, direction in source.bindings
    }
    for identity in selected:
        span = spans[identity]
        member = next(m for m in source.members if m.identifier == identity)
        if member is not span:
            raise ValueError("cover_source_metadata_replaced")
        band, direction = bindings[identity]
        if tuple(source.bands[band][::direction]) != span.nodes:
            raise ValueError("cover_original_direction_lost")
        a, b = (local[node] for node in span.nodes)
        dx, dy = b - a
        offset = span.width_m / 2 * np.array([-dy, dx]) / np.hypot(dx, dy)
        original = shapely.Polygon([a - offset, b - offset, b + offset, a + offset])
        missing_areas.append(original.difference(domain).area)
        sections.extend(shapely.LineString([p - offset, p + offset]) for p in (a, b))
    if max(missing_areas) > 1e-8:
        raise ValueError("cover_original_band_not_covered")
    if any(not domain.buffer(1e-8).covers(line) for line in sections):
        raise ValueError("cover_original_end_profile_not_covered")
    points = mesh.vertices[mesh.triangles]
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    if not np.isfinite(points).all() or np.any(normals[:, 2] <= 0):
        raise ValueError("cover_invalid_finite_surface")
    polygons = shapely.polygons(points[:, :, :2])
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("cover_overlapping_or_nonconforming_surface")
    complete = shapely.coverage_union_all(polygons)
    if complete.symmetric_difference(domain).area > 1e-8:
        raise ValueError("cover_finite_domain_changed")
    edges = Counter(
        tuple(sorted((int(a), int(b))))
        for face in mesh.triangles
        for a, b in pairwise([*face, face[0]])
    )
    if (
        any(v not in (1, 2) for v in edges.values())
        or len(mesh.vertices) - len(edges) + len(mesh.triangles) != 1
    ):
        raise ValueError("cover_invalid_surface_topology")
    gradients = -normals[:, :2] / normals[:, 2, None]
    actual_grade = float(np.linalg.norm(gradients, axis=1).max())
    if actual_grade > min(0.025, source.maximum_gradient) * 1.0000001:
        raise ValueError("cover_finite_grade_exceeded")
    field = result["field"]
    observations = result["observations"]
    error = float(
        abs(
            field.intercept
            + (observations[:, :2] - field.origin) @ field.gradient
            - observations[:, 2]
        ).max()
    )
    if error > 0.5:
        raise ValueError("cover_original_height_observation_lost")
    return {
        "original_members": len(selected),
        "original_end_profiles": len(sections),
        "original_source_nodes": len(nodes.keys() & set(source.node_ids)),
        "maximum_original_band_missing_area_m2": max(missing_areas),
        "original_profile_xy_tolerance_m": 1e-8,
        "domain_area_m2": domain.area,
        "vertices": len(mesh.vertices),
        "triangles": len(mesh.triangles),
        "mesh_euler_characteristic": 1,
        "maximum_finite_isotropic_grade": actual_grade,
        "maximum_all_original_observation_error_m": error,
        "scope": "Full rectangles and twelve original end profiles retained; unchanged member objects, original direction mapping, coverage/manifold and finite affine heights. No driving-turn, logical routing, original end authority or Source/World/device admission.",
    }
