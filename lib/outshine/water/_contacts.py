"""Bounded physical point contacts on the caller's selected shared water surface."""

from dataclasses import dataclass

import numpy as np

from ..geometry import SurfaceSampler


@dataclass(frozen=True)
class PointContacts:
    points: np.ndarray
    faces: np.ndarray
    weights: np.ndarray


def prepare_contacts(mesh, requested):
    """Map exact XY positions; never move a point onto a nearby edge or sheet."""
    empty = PointContacts(np.empty((0, 3)), np.empty(0, dtype=int), np.empty((0, 3)))
    if requested is None:
        return empty, {"point_contacts": 0, "point_contact_candidates": 0}
    if not isinstance(requested, (np.ndarray, list, tuple)):
        raise TypeError("finite N by 3 ENU contact coordinates required")
    if isinstance(requested, np.ndarray) and requested.ndim == 0:
        raise ValueError("finite N by 3 ENU contact coordinates required")
    if len(requested) > 4096:
        return None, {
            "reason": "point_contact_budget_exhausted",
            "point_contacts": len(requested),
        }
    if isinstance(requested, (list, tuple)):
        for row in requested:
            valid = (isinstance(row, np.ndarray) and row.shape == (3,)) or (
                isinstance(row, (list, tuple)) and len(row) == 3
            )
            if not valid or any(
                not isinstance(x, (int, float, np.integer, np.floating)) for x in row
            ):
                raise ValueError("three numeric ENU coordinates per contact required")
    raw = np.asarray(requested)
    if len(raw) == 0 and raw.ndim == 1:
        return empty, {"point_contacts": 0, "point_contact_candidates": 0}
    if raw.ndim != 2 or raw.shape[1] != 3 or raw.dtype.kind not in "fiu":
        raise ValueError("finite numeric N by 3 ENU contact coordinates required")
    points = np.array(raw, dtype=float, copy=True)
    if not np.isfinite(points).all():
        raise ValueError("finite ENU contact coordinates required")
    if not len(points):
        return empty, {"point_contacts": 0, "point_contact_candidates": 0}
    sampler = SurfaceSampler(mesh, xy_tolerance_m=0, max_faces=16384)
    sampled = sampler.sample(points[:, :2], max_queries=4096, max_candidates=262144)
    report = {
        "point_contacts": len(points),
        "point_contact_candidates": sampled.report["candidates"],
    }
    rejected = [{"index": i, "reason": r} for i, r in enumerate(sampled.reasons) if r]
    if rejected:
        return None, dict(
            report,
            reason="unresolved_point_contacts",
            unresolved_point_contacts=rejected,
        )
    triangle = mesh.vertices[mesh.triangles[sampled.face_indices], :2]
    edges = triangle[:, 1:] - triangle[:, :1]
    try:
        uv = np.linalg.solve(
            np.swapaxes(edges, 1, 2), (points[:, :2] - triangle[:, 0])[..., None]
        )[..., 0]
    except np.linalg.LinAlgError:
        return None, dict(report, reason="unrepresentable_point_contact_equations")
    if not np.isfinite(uv).all():
        return None, dict(report, reason="unrepresentable_point_contact_equations")
    weights = np.c_[1 - uv.sum(1), uv]
    faces = sampled.face_indices.copy()
    for array in (points, faces, weights):
        array.setflags(write=False)
    report["point_contact_prepared_bytes"] = (
        points.nbytes + faces.nbytes + weights.nbytes
    )
    return PointContacts(points, faces, weights), report


def contact_equations(mesh, contacts, unknowns, datum):
    from scipy import sparse

    count = len(contacts.points)
    matrix = sparse.coo_matrix(
        (
            contacts.weights.ravel(),
            (np.repeat(np.arange(count), 3), mesh.triangles[contacts.faces].ravel()),
        ),
        shape=(count, unknowns),
    ).tocsc()
    right = contacts.points[:, 2] - datum * contacts.weights.sum(axis=1)
    return matrix, right


def point_contact_errors(mesh, contacts):
    """Reconstruct physical planes independently of the fitted barycentric rows."""
    triangle = mesh.vertices[mesh.triangles[contacts.faces]]
    normal = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    residual = abs(np.sum((contacts.points - triangle[:, 0]) * normal, axis=1))
    return {
        "fixed_point_up_error_m": float((residual / abs(normal[:, 2])).max(initial=0)),
        "fixed_point_distance_m": float(
            (residual / np.linalg.norm(normal, axis=1)).max(initial=0)
        ),
    }
