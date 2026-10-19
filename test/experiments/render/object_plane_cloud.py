"""Whole-face object-space chart proposal; no visibility or material admission."""

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class Chart:
    faces: np.ndarray
    basis: np.ndarray
    offset: float
    low: np.ndarray
    shape: tuple
    pitch: float

    @property
    def texels(self):
        return self.shape[0] * self.shape[1]


REASONS = {
    0: "chart",
    1: "degenerate",
    2: "plane_budget",
    3: "relation_budget",
    4: "small_group",
    5: "texture_dimensions",
    6: "texture_budget",
}


def compact_regions(projected, faces, pitch, work):
    """Balanced spatial splits only when padded rectangles save declared bytes.

    This is a local heuristic, not an optimal packing claim. Each split owns
    disjoint whole original faces; its total node count is below twice that
    number. Overlapping triangles are never clipped or discarded by this step.
    """
    pending = [(projected, faces)]
    result = []
    while pending:
        points, ids = pending.pop()
        work["region_face_bounds"] += len(ids)
        low = np.floor(points.min((0, 1)) / pitch) * pitch - pitch
        high = np.ceil(points.max((0, 1)) / pitch) * pitch + pitch
        if len(ids) < 6:
            result.append((ids, low, high))
            continue
        axis = int(np.argmax(high - low))
        order = np.argsort(points[:, :, axis].mean(1), kind="stable")
        middle = len(order) // 2
        parts = (order[:middle], order[middle:])
        parent = np.prod(np.rint((high - low) / pitch))
        children = 0
        for part in parts:
            q = points[part]
            lo = np.floor(q.min((0, 1)) / pitch) - 1
            hi = np.ceil(q.max((0, 1)) / pitch) + 1
            children += np.prod(hi - lo)
        # Two descriptors instead of one; mip costs are checked in the report.
        if 12 * (parent - children) > 112:
            pending.extend((points[part], ids[part]) for part in reversed(parts))
        else:
            result.append((ids, low, high))
    return result


def partition(
    positions,
    indices,
    *,
    epsilon,
    pitch,
    plane_limit=256,
    relation_limit=4194304,
    side_limit=1024,
    texel_limit=4194304,
    compact=True,
):
    """Assign each original triangle to one chart or unchanged fallback.

    A seed face chooses an oriented plane. Every vertex of an accepted face
    must lie in its epsilon slab; compatible orientation avoids folding faces
    into the opposite side. Rectangle textures include one exterior guard texel.
    These tests do not certify the later sampled texture or depth/lighting error.
    """
    xyz, ids = np.asarray(positions, float), np.asarray(indices)
    if (
        xyz.ndim != 2
        or xyz.shape[1] != 3
        or len(xyz) > 262144
        or not np.isfinite(xyz).all()
        or (abs(xyz) > 1e9).any()
        or ids.ndim != 2
        or ids.shape[1] != 3
        or len(ids) > 131072
        or ids.dtype.kind not in "iu"
        or (ids < 0).any()
        or (ids >= len(xyz)).any()
    ):
        raise ValueError(
            "bounded finite vertices and original triangle indices required"
        )
    if not all(np.isfinite(x) and x >= 1e-6 for x in (epsilon, pitch)):
        raise ValueError("finite slab/texel sizes >= 1 micrometre required")
    if type(compact) is not bool:
        raise ValueError("explicit compact-region choice required")
    for value, maximum in (
        (plane_limit, 256),
        (relation_limit, 4194304),
        (side_limit, 1024),
        (texel_limit, 4194304),
    ):
        if type(value) is not int or not 0 <= value <= maximum:
            raise ValueError("explicit bounded integer work limits required")
    triangles = xyz[ids]
    cross = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    area2 = np.linalg.norm(cross, axis=1)
    normals = np.divide(
        cross, area2[:, None], out=np.zeros_like(cross), where=area2[:, None] > 0
    )
    pending = area2 > 0
    reasons = np.where(pending, 2, 1).astype(np.uint8)
    owner = np.full(len(ids), -1, np.int32)
    order = np.argsort(-area2, kind="stable")
    charts, relations, texels, attempts = [], 0, 0, 0
    work = {"region_face_bounds": 0}
    for seed in order:
        if not pending[seed]:
            continue
        if attempts == plane_limit or len(charts) == plane_limit:
            break
        candidates = np.flatnonzero(pending)
        if len(candidates) > relation_limit - relations:
            reasons[pending] = 3
            break
        attempts += 1
        relations += len(candidates)
        n = normals[seed]
        offset = float(triangles[seed, 0] @ n)
        distances = triangles[candidates] @ n - offset
        compatible = (abs(distances).max(1) <= epsilon) & (
            normals[candidates] @ n >= 0.5
        )
        faces = candidates[compatible]
        pending[faces] = False
        if len(faces) < 3:
            reasons[faces] = 4
            continue
        axis = np.eye(3)[np.argmin(abs(n))]
        u = np.cross(axis, n)
        u /= np.linalg.norm(u)
        basis = np.stack((u, np.cross(n, u), n))
        projected = triangles[faces] @ basis[:2].T
        regions = (
            compact_regions(projected, faces, pitch, work)
            if compact
            else [
                (
                    faces,
                    np.floor(projected.min((0, 1)) / pitch) * pitch - pitch,
                    np.ceil(projected.max((0, 1)) / pitch) * pitch + pitch,
                )
            ]
        )
        for members, low, high in regions:
            if len(charts) == plane_limit:
                reasons[members] = 2
                continue
            width, height = np.rint((high - low) / pitch).astype(np.int64)
            if min(width, height) <= 0 or max(width, height) > side_limit:
                reasons[members] = 5
                continue
            count = int(width * height)
            if count > texel_limit - texels:
                reasons[members] = 6
                continue
            owner[members] = len(charts)
            reasons[members] = 0
            charts.append(
                Chart(
                    members.astype(np.uint32),
                    basis,
                    offset,
                    low,
                    (int(height), int(width)),
                    pitch,
                )
            )
            texels += count
    fallback = np.flatnonzero(owner < 0).astype(np.uint32)
    assigned = np.concatenate([*(c.faces for c in charts), fallback])
    if not np.array_equal(np.sort(assigned), np.arange(len(ids))):
        raise ValueError("incomplete or duplicated original-face assignment")
    return (
        tuple(charts),
        fallback,
        reasons,
        {
            "input_triangles": len(ids),
            **work,
            "plane_attempts": attempts,
            "face_plane_relations": relations,
            "vertex_plane_dots": 3 * relations,
            "charts": len(charts),
            "chart_texels": texels,
            "fallback_triangles": len(fallback),
            "proposal_quad_triangle_equivalents": 2 * len(charts),
            "reasons": {
                name: int((reasons == code).sum()) for code, name in REASONS.items()
            },
        },
    )


def mip_texels(shape):
    """Exact declared 2D floor-halving mip layout, including the base level."""
    h, w = shape
    total = h * w
    while h > 1 or w > 1:
        h, w = max(1, h // 2), max(1, w // 2)
        total += h * w
    return total
