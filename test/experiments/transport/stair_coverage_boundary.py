"""Research: prove a single simple boundary before the general coverage check."""

import numpy as np
import shapely
from outshine_ref.geometry.surface_patches._coverage import proves_single_boundary

GENERAL_COVERAGE = shapely.coverage_is_valid


def witness(xy, faces, polygons, *, ring_batch=None):
    proved = proves_single_boundary(xy, faces, polygons, ring_batch=ring_batch)
    return {
        "proved": proved,
        "reason": "positive_chain_simple_boundary"
        if proved
        else "general_coverage_required",
        "input_triangles": len(faces),
        "orientation_batch_triangles": len(polygons)
        if ring_batch is None
        else ring_batch,
    }


def polygon_witness(polygons):
    """Conservative adapter; production code could reuse existing XY and indices."""
    polygons = np.asarray(polygons, dtype=object)
    if (
        polygons.ndim != 1
        or not 1 <= len(polygons) <= 65536
        or not np.all(shapely.get_type_id(polygons) == 3)
        or not np.all(shapely.get_num_coordinates(polygons) == 4)
    ):
        return {"proved": False, "reason": "not_bounded_triangles"}
    coords = shapely.get_coordinates(polygons).reshape(-1, 4, 2)
    if (
        not np.isfinite(coords).all()
        or np.max(abs(coords), initial=0) > 1e9
        or not np.array_equal(coords[:, 0], coords[:, -1])
    ):
        return {"proved": False, "reason": "not_finite_closed_triangles"}
    xy, inverse = np.unique(coords[:, :3].reshape(-1, 2), axis=0, return_inverse=True)
    if len(xy) > 65536:
        return {"proved": False, "reason": "vertex_budget"}
    faces = inverse.reshape(-1, 3)
    if np.any(np.diff(np.sort(faces, axis=1), axis=1) == 0):
        return {"proved": False, "reason": "repeated_triangle_vertex"}
    return witness(xy, faces, polygons)


def mesh_witness(mesh, polygons, *, ring_batch=None):
    """Reuse the strict constructor's owned mesh instead of decoding polygon coordinates."""
    if not 1 <= len(mesh.triangles) <= 65536 or len(mesh.vertices) > 65536:
        return {"proved": False, "reason": "mesh_budget"}
    xy, inverse = np.unique(mesh.vertices[:, :2], axis=0, return_inverse=True)
    return witness(xy, inverse[mesh.triangles], polygons, ring_batch=ring_batch)


def checked_coverage(polygons, *, records=None):
    proof = polygon_witness(polygons)
    result = proof["proved"] or bool(GENERAL_COVERAGE(polygons))
    if records is not None:
        records.append(dict(proof, valid=result, input_polygons=len(polygons)))
    return result
