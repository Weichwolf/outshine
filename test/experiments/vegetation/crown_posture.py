"""Length-preserving directed curvature on a saved, parent-ordered skeleton."""

from itertools import pairwise

import numpy as np
from form_plan import validate_parents
from scipy.spatial.transform import Rotation


def posture(
    positions, parents, mobility, direction, curvature, max_turn=0.35, method="layers"
):
    positions = np.asarray(positions, dtype=float)
    parents, mobility = np.asarray(parents), np.asarray(mobility, dtype=float)
    direction = np.asarray(direction, dtype=float)
    validate_parents(parents)
    count = len(parents)
    if (
        method not in ("layers", "serial")
        or not 2 <= count <= 65536
        or positions.shape != (count, 3)
        or mobility.shape != (count,)
        or direction.shape != (3,)
        or not np.isfinite(positions).all()
        or np.max(abs(positions)) > 100000
        or not np.isfinite(mobility).all()
        or np.any((mobility < 0) | (mobility > 1))
        or not np.isfinite(direction).all()
        or abs(np.linalg.norm(direction) - 1) > 1e-12
        or not np.isfinite(curvature)
        or not 0 <= curvature <= 2
        or not np.isfinite(max_turn)
        or not 0 < max_turn <= 0.5
    ):
        raise ValueError(
            "bounded metre skeleton, mobility, unit target and curvature required"
        )
    delta = positions[1:] - positions[parents[1:]]
    length = np.linalg.norm(delta, axis=1)
    if np.any(length <= 1e-8):
        raise ValueError("noncollapsed source segments required")
    moved = positions.copy()
    rotations = np.tile(np.eye(3), (count, 1, 1))
    if curvature == 0 or not np.any(mobility[1:]):
        return moved, rotations
    depth = np.zeros(count, np.int32)
    for node, parent in enumerate(parents[1:], 1):
        depth[node] = depth[parent] + 1
    order = np.argsort(depth, kind="stable")
    boundaries = np.r_[0, np.flatnonzero(np.diff(depth[order])) + 1, count]
    groups = (
        (order[first:last] for first, last in pairwise(boundaries[1:]))
        if method == "layers"
        else (np.array([node]) for node in range(1, count))
    )
    for nodes in groups:
        parent = parents[nodes]
        inherited = rotations[parent]
        edges = np.einsum("nij,nj->ni", inherited, delta[nodes - 1])
        tangent = edges / length[nodes - 1, None]
        cross = np.cross(tangent, direction)
        sine = np.linalg.norm(cross, axis=1)
        theta = np.arctan2(sine, tangent @ direction)
        strength = curvature * length[nodes - 1] * mobility[nodes]
        final = 2 * np.arctan2(np.exp(-strength) * np.sin(theta / 2), np.cos(theta / 2))
        angle = np.minimum(np.maximum(theta - final, 0), max_turn)
        angle[strength == 0] = 0
        scale = np.divide(angle, sine, out=np.zeros_like(sine), where=sine > 1e-14)
        turn = Rotation.from_rotvec(cross * scale[:, None]).as_matrix()
        rotations[nodes] = turn @ inherited
        moved[nodes] = moved[parent] + np.einsum("nij,nj->ni", turn, edges)
    return moved, rotations


def pose_rows(rows, owners, source, moved, rotations):
    """Rigid organ groups retain local shapes, area, anchors and logical IDs."""
    owners = np.asarray(owners)
    if (
        owners.shape != (len(rows["origins"]),)
        or owners.dtype.kind not in "iu"
        or np.any((owners < 0) | (owners >= len(source)))
    ):
        raise ValueError("one valid saved owner per demanded organ required")
    result = dict(rows)
    r = rotations[owners]
    for key in ("origins", "nodes"):
        if key in rows:
            result[key] = moved[owners] + np.einsum(
                "nij,nj->ni", r, rows[key] - source[owners]
            )
    result["frames"] = r @ rows["frames"]
    return result


def pose_group(crown, moved, rotations, node, max_leaves):
    rows = crown.group(node, max_leaves)
    if rows is None:
        return None
    return pose_rows(rows, rows["owner_nodes"], crown.positions, moved, rotations)
