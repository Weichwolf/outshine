"""Bounded shared-origin position streams; neighboring Float32 payloads are owners."""

import numpy as np
from stair_regional_precision import position_payload


def checked_anchor(group, count, dimension, used):
    if not isinstance(group, tuple) or len(group) != 3:
        raise ValueError("explicit neighbor vertex IDs, payload and origin required")
    ids, positions = map(np.asarray, group[:2])
    origin = np.asarray(group[2], float)
    if (
        ids.ndim != 1
        or not 1 <= len(ids) <= count
        or ids.dtype.kind not in "iu"
        or ids.min() < 0
        or ids.max() >= count
        or len(np.unique(ids)) != len(ids)
        or used.intersection(map(int, ids))
        or positions.dtype != np.dtype("f4")
        or positions.shape != (len(ids), dimension)
        or origin.shape != (dimension,)
        or not np.isfinite(positions).all()
        or not np.isfinite(origin).all()
    ):
        raise ValueError(
            "bounded disjoint neighbor bindings with actual Float32 payload required"
        )
    used.update(map(int, ids))
    return ids.astype(np.intp, copy=False), positions, origin.astype(float, copy=False)


def encode_regions(points, origin, anchors=(), *, method="rebase_if_exact"):
    if method not in ("copy", "rebase_if_exact") or len(anchors) > 32:
        raise ValueError("known bounded shared-origin encoding required")
    origin = np.asarray(origin, float)
    positions = position_payload(points, origin)
    table, ids, used = [origin], None, set()
    for group in anchors:
        selected, peer, peer_origin = checked_anchor(
            group, len(positions), positions.shape[1], used
        )
        remaining = np.ones(len(selected), bool)
        if method == "rebase_if_exact":
            world = peer.astype(float)
            world += peer_origin
            rebased = np.empty(peer.shape, dtype="f4")
            np.subtract(world, origin, out=rebased)
            restored = rebased.astype(float)
            restored += origin
            exact = np.all(restored == world, axis=1)
            positions[selected[exact]] = rebased[exact]
            remaining &= ~exact
        if not remaining.any():
            continue
        owner = next(
            (i for i, row in enumerate(table) if np.array_equal(row, peer_origin)), None
        )
        if owner is None:
            owner = len(table)
            table.append(peer_origin)
        if owner:
            if ids is None:
                ids = np.zeros(len(positions), dtype="u1")
            ids[selected[remaining]] = owner
        positions[selected[remaining]] = peer[remaining]
    origins = np.array(table, dtype=float)
    return positions, origins, ids if ids is not None else np.empty(0, dtype="u1")


def reconstruct(positions, origins, origin_ids):
    positions, origins, origin_ids = map(np.asarray, (positions, origins, origin_ids))
    if (
        positions.ndim != 2
        or positions.shape[1] not in (2, 3)
        or not 1 <= len(positions) <= 65536
        or positions.dtype != np.dtype("f4")
        or origins.ndim != 2
        or origins.shape[1] != positions.shape[1]
        or not 1 <= len(origins) <= 33
        or origins.dtype != np.dtype("f8")
        or origin_ids.dtype != np.dtype("u1")
        or (len(origins) == 1 and origin_ids.shape != (0,))
        or (len(origins) > 1 and origin_ids.shape != (len(positions),))
        or (origin_ids.size and origin_ids.max() >= len(origins))
        or not np.isfinite(positions).all()
        or not np.isfinite(origins).all()
    ):
        raise ValueError("complete bounded finite position/origin streams required")
    result = positions.astype(float)
    result += origins[origin_ids] if origin_ids.size else origins[0]
    if not np.isfinite(result).all():
        raise ValueError("reconstructed regional coordinates must remain finite")
    return result
