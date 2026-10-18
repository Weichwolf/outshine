"""Bounded original continuous-phase evaluation, no representative phase bins."""

import numpy as np
from outshine_ref.vegetation.shoot_plan import normal_frame
from outshine_ref.vegetation.vector3 import cross3


def frame32(direction):
    """Float32 counterpart of the original Z-hint frame, including pole fallback."""
    direction = direction / np.linalg.norm(direction, axis=-1, keepdims=True)
    hint = np.array((0, 0, 1), np.float32)
    lateral = cross3(direction, hint)
    fallback = np.eye(3, dtype=np.float32)[np.argmin(abs(direction), axis=-1)]
    fallback = cross3(direction, fallback)
    lateral = np.where(
        np.linalg.norm(lateral, axis=-1, keepdims=True) < np.float32(1e-8),
        fallback,
        lateral,
    )
    lateral /= np.linalg.norm(lateral, axis=-1, keepdims=True)
    return np.stack((lateral, direction, cross3(lateral, direction)), axis=-1)


def local_poses(data, family, phases):
    dtype = phases.dtype
    centre, tangent, lateral, normal = data[f"arc{family}"]
    indices = np.arange(len(centre), dtype=dtype)
    increment = np.asarray(np.pi * (3 - np.sqrt(5)), dtype=dtype)
    angle = phases[:, None] + indices[None] * increment
    radial = np.cos(angle)[..., None] * lateral + np.sin(angle)[..., None] * normal
    direction = (
        np.asarray(np.cos(1.12), dtype=dtype) * tangent
        + np.asarray(np.sin(1.12), dtype=dtype) * radial
    )
    hint = np.array((0, 0, 1), dtype=dtype)
    direction -= (
        np.asarray(0.3, dtype=dtype)
        * np.sum(direction * hint, axis=-1, keepdims=True)
        * hint
    )
    direction /= np.linalg.norm(direction, axis=-1, keepdims=True)
    flat = direction.reshape(-1, 3)
    frames = (
        normal_frame(flat, (0, 0, 1))
        if dtype == np.dtype(np.float64)
        else frame32(flat)
    )
    frames = frames.reshape(*direction.shape[:2], 3, 3)
    origins = centre + data["petiole"] * direction
    return origins, frames


def evaluate(data, selected, *, max_leaves, anchors=False, batch_hosts=256):
    if type(max_leaves) is not int or not 0 <= max_leaves <= 100000:
        raise ValueError("explicit leaf capacity up to100000 required")
    if type(batch_hosts) is not int or not 1 <= batch_hosts <= 4096:
        raise ValueError("bounded positive host batch required")
    selected = np.asarray(selected)
    if selected.dtype != np.dtype(bool) or selected.shape != data["hosts"].shape:
        raise ValueError("one Boolean demand per original host required")
    hosts = np.flatnonzero(selected)
    counts = np.diff(data["offsets"])[hosts]
    starts = np.r_[0, np.cumsum(counts, dtype=np.int64)]
    total = int(starts[-1])
    if total > max_leaves:
        return None
    dtype = data["origins"].dtype
    result = {
        "ids": np.empty(total, np.uint64),
        "variants": np.empty(total, np.uint8),
        "origins": np.empty((total, 3), dtype),
        "frames": np.empty((total, 3, 3), dtype),
    }
    if anchors:
        result["nodes"] = np.empty((total, 3), dtype)
    for family in np.unique(counts):
        if not family:
            continue
        chosen = np.flatnonzero(counts == family)
        organ = np.arange(int(family), dtype=np.int64)
        for begin in range(0, len(chosen), batch_hosts):
            take = chosen[begin : begin + batch_hosts]
            h = hosts[take]
            target = starts[take, None] + organ
            ids = data["offsets"][h, None] + organ.astype(np.uint64)
            origin, frames = local_poses(data, int(family), data["phases"][h])
            stem = data["frames"][h]
            result["ids"][target] = ids
            result["variants"][target] = data["variants"][ids]
            result["origins"][target] = (
                np.matmul(origin, stem.swapaxes(-1, -2)) + data["origins"][h, None]
            )
            result["frames"][target] = np.einsum("hij,hnjk->hnik", stem, frames)
            if anchors:
                centre = data[f"arc{int(family)}"][0]
                result["nodes"][target] = (
                    np.matmul(centre[None], stem.swapaxes(-1, -2))
                    + data["origins"][h, None]
                )
    for value in result.values():
        value.setflags(write=False)
    return result
