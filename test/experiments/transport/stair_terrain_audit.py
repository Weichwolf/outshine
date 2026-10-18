"""Saved-surface/whole-solid diagnostics; no fit, triangulation or excavation."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance
from stair_network_geometry import vertex_links
from stair_terrain_solid import manifold
from surface_solid_checks import audit_closed


def top_surface(mesh):
    p = mesh.vertices[mesh.triangles]
    normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    lengths = np.linalg.norm(normals, axis=1)
    selected = normals[:, 2] > lengths * 1e-8
    return Mesh(mesh.vertices, mesh.triangles[selected])


def clearance(lower, upper):
    result = vertical_clearance(
        lower, upper, max_faces=32768, max_pairs=1000000, seconds=20
    )
    if not result["valid"]:
        raise ValueError("saved terrain separation unresolved: " + str(result))
    i, j = result["face_pair"]
    a = shapely.Polygon(lower.vertices[lower.triangles[i], :2])
    b = shapely.Polygon(upper.vertices[upper.triangles[j], :2])
    result["witness_projected_overlap_area_m2"] = a.intersection(b).area
    return result


def body_proof(mesh, *, max_faces=65536):
    if type(max_faces) is not int or not 1 <= max_faces <= 262144:
        raise ValueError("bounded explicit terrain-body face capacity required")
    if len(mesh.vertices) > 65536 or len(mesh.triangles) > max_faces:
        raise ValueError("saved terrain body exceeds audit caps")
    p = mesh.vertices[mesh.triangles]
    normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    return {
        "vertices": len(mesh.vertices),
        "faces": len(mesh.triangles),
        "array_bytes": mesh.vertices.nbytes + mesh.triangles.nbytes,
        "closed_oriented_volume_m3": audit_closed(mesh),
        "zero_area_faces": int((np.linalg.norm(normals, axis=1) == 0).sum()),
        **vertex_links(mesh.vertices, mesh.triangles),
    }


def inspect(retaining, excavated, ground, upper, road):
    quantized_road = Mesh(
        road.vertices.astype(np.float32).astype(float), road.triangles
    )
    lower = Mesh(ground.vertices.astype(np.float32).astype(float), ground.triangles)
    higher = Mesh(upper.vertices.astype(np.float32).astype(float), upper.triangles)
    original = Mesh(
        retaining.vertices.astype(np.float32).astype(float), retaining.triangles
    )
    overlap = manifold(excavated) ^ manifold(quantized_road)
    return {
        "retaining_double": body_proof(retaining),
        "retaining_float32": body_proof(original),
        "excavated_float32": body_proof(excavated),
        "excavated_positions_exact_float32": bool(
            np.array_equal(excavated.vertices, excavated.vertices.astype(np.float32))
        ),
        "retaining_to_bridge_double": clearance(top_surface(retaining), upper),
        "retaining_to_bridge_float32": clearance(top_surface(original), higher),
        "excavated_to_bridge_float32": clearance(top_surface(excavated), higher),
        "excavated_to_ground_float32": clearance(top_surface(excavated), lower),
        "excavated_full_road_overlap_m3": overlap.volume(),
        "overlap_oracle_status": str(overlap.status()),
        "explicit_overlap_input_copy_bytes": (
            excavated.vertices.nbytes
            + excavated.triangles.nbytes
            + quantized_road.vertices.nbytes
            + quantized_road.triangles.nbytes
        ),
        "overlap_scope": "Same Manifold kernel as excavation; not independent exact arithmetic.",
        "approved_terrain": False,
        "reason": "Finite-precision whole contact and coherent terrain shaping remain unresolved.",
    }
