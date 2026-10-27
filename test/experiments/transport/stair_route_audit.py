"""Independent floating 3D plane/edge-distance checks on a constructed route."""

import numpy as np


def audit_section(mesh, section):
    maximum_plane, maximum_outside = 0.0, 0.0
    tests = 0
    xyz = section["xyz_m"]
    scale = max(
        1.0, float(abs(mesh.vertices).max(initial=0)), float(abs(xyz).max(initial=0))
    )
    guard = 64 * np.finfo(float).eps * scale
    for index, face_ids in enumerate(section["segment_face_ids"]):
        points = xyz[index : index + 2]
        for face_id in face_ids:
            triangle = mesh.vertices[mesh.triangles[face_id]]
            normal = np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0])
            length = np.linalg.norm(normal)
            if length == 0:
                raise ValueError("route provenance contains a degenerate 3D face")
            normal /= length
            maximum_plane = max(
                maximum_plane, float(abs((points - triangle[0]) @ normal).max())
            )
            for a, b in zip(triangle, np.roll(triangle, -1, axis=0), strict=True):
                edge = b - a
                signed = np.cross(edge, points - a) @ normal / np.linalg.norm(edge)
                maximum_outside = max(
                    maximum_outside, float(np.maximum(-signed, 0).max())
                )
            tests += 2
    if maximum_plane > guard or maximum_outside > guard:
        raise ValueError("route leaves its explicitly recorded actual 3D support faces")
    return {
        "face_endpoint_tests": tests,
        "maximum_plane_residual_m": maximum_plane,
        "maximum_negative_edge_distance_m": maximum_outside,
        "floating_arithmetic_guard_m": guard,
        "scope": "Independent 3D cross-product plane and triangle-halfspace residuals at both segment ends. Convex faces cover each straight segment. Floating residual check, not an exact interval certificate or physical clearance allowance.",
    }
