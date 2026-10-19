"""Offline opaque surface support for an explicit convex camera-position box."""

import itertools
from dataclasses import dataclass

import numpy as np


@dataclass
class Support:
    keep: np.ndarray
    witness: np.ndarray
    blockers: np.ndarray
    work: dict


def corners(box):
    box = np.asarray(box, float)
    if box.shape != (2, 3) or not np.isfinite(box).all() or np.any(box[0] > box[1]):
        raise ValueError("finite ordered camera-position box required")
    return np.array(
        [box[index, np.arange(3)] for index in itertools.product((0, 1), repeat=3)]
    )


def umbra(triangle, observers, margin):
    """Inward halfspaces common to all corner-eye shadows of one triangle.

    A point inside these cones is blocked for each corner. Its cone of blocked
    eye positions is convex, so the convex hull of the corners is blocked too.
    The final halfspace enforces strict depth ordering behind the blocker plane.
    """
    a, b, c = triangle
    normal = np.cross(b - a, c - a)
    length = np.linalg.norm(normal)
    if length == 0:
        return None
    normal /= length
    if np.min((observers - a) @ normal) <= margin:
        return None
    normals, anchors = [], []
    centre = triangle.mean(0)
    for start, end in ((a, b), (b, c), (c, a)):
        side = np.cross(end - start, observers - start)
        norm = np.linalg.norm(side, axis=1)
        if np.any(norm == 0):
            return None
        side /= norm[:, None]
        orientation = side @ (centre - start)
        if np.any(abs(orientation) <= margin):
            return None
        side *= np.where(orientation >= 0, 1, -1)[:, None]
        normals.extend(side)
        anchors.extend([start] * len(side))
    normals.append(-normal)
    anchors.append(a)
    return np.array(normals), np.array(anchors)


def select_support(
    positions, indices, observer_box, *, max_blockers=128, batch=256, near_radius_m=0.2
):
    """Keep uncertain surfaces; bounded blocker budget affects savings, not coverage.

    Only wholly back-facing or strictly inside one original triangle's umbra is
    omitted. Chosen blockers themselves stay resident, simplifying provenance.
    This is not a filtered field, raster proof, shadow PVS or native runtime path.
    """
    original_positions = np.asarray(positions)
    positions, indices = np.asarray(original_positions, float), np.asarray(indices)
    observers = corners(observer_box)
    if (
        positions.ndim != 2
        or positions.shape[1:] != (3,)
        or not np.isfinite(positions).all()
        or indices.ndim != 2
        or indices.shape[1:] != (3,)
        or not np.issubdtype(indices.dtype, np.integer)
        or np.any(indices < 0)
        or np.any(indices >= len(positions))
        or len(indices) > 131072
        or not 0 <= max_blockers <= 512
        or not 1 <= batch <= 1024
        or not np.isfinite(near_radius_m)
        or near_radius_m < 0
    ):
        raise ValueError("bounded finite indexed triangle input required")
    triangle = positions[indices]
    normal = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    area = np.linalg.norm(normal, axis=1)
    normal = np.divide(
        normal, area[:, None], out=np.zeros_like(normal), where=area[:, None] > 0
    )
    centre = np.mean(observers, axis=0)
    half = np.ptp(observers, axis=0) / 2
    distance = np.einsum("ij,ij->i", centre - triangle[:, 0], normal)
    reserve = abs(normal) @ half
    scale = max(1.0, float(abs(positions).max(initial=0)), float(abs(observers).max()))
    margin = 1e-8 * scale
    back = (area > 0) & (distance + reserve < -margin)
    # A near-clipped blocker cannot hide distant geometry in the actual query.
    certain_front = (area > 0) & (distance - reserve > max(margin, near_radius_m))
    gap = np.maximum(abs(triangle.mean(1) - centre) - half, 0)
    score = area / np.maximum(np.sum(gap * gap, axis=1), margin * margin)
    candidates = np.flatnonzero(certain_front)
    ranked = candidates[np.argsort(-score[candidates], kind="stable")][:max_blockers]
    keep = ~back
    witness = np.full(len(indices), -1, np.int32)
    witness[back] = -2
    protected = np.zeros(len(indices), bool)
    protected[ranked] = True
    work = {
        "input_triangles": len(indices),
        "back_faces": int(back.sum()),
        "blockers_tested": 0,
        "triangle_blocker_tests": 0,
        "halfspace_vertex_tests": 0,
        "largest_test_array_bytes": 0,
        "margin_m": margin,
        "near_radius_m": near_radius_m,
        "input_position_copy_bytes": 0
        if np.shares_memory(positions, original_positions)
        else positions.nbytes,
        "triangle_gather_bytes": triangle.nbytes,
    }
    for blocker in ranked:
        planes = umbra(triangle[blocker], observers, margin)
        if planes is None:
            continue
        normals, anchors = planes
        pending = np.flatnonzero(keep & ~protected)
        work["blockers_tested"] += 1
        for start in range(0, len(pending), batch):
            faces = pending[start : start + batch]
            # Subtract a local anchor first; avoid cancellation in n.P - n.A.
            delta = triangle[faces, :, None, :] - anchors[None, None, :, :]
            values = np.einsum("bvhj,hj->bvh", delta, normals)
            hidden = np.all(values > margin, axis=(1, 2))
            keep[faces[hidden]] = False
            witness[faces[hidden]] = blocker
            work["triangle_blocker_tests"] += len(faces)
            work["halfspace_vertex_tests"] += values.size
            work["largest_test_array_bytes"] = max(
                work["largest_test_array_bytes"], delta.nbytes + values.nbytes
            )
    work["occluded_faces"] = int((witness >= 0).sum())
    work["kept_faces"] = int(keep.sum())
    work["selected_blockers"] = len(ranked)
    return Support(keep, witness, ranked.astype(np.int32), work)
