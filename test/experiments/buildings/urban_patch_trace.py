"""Independent exact box and analytic plane-mask consumers of ready city fields."""

import numpy as np
from urban_patches import descriptors

NORMALS = np.array(
    [[-1, 0, 0], [1, 0, 0], [0, -1, 0], [0, 1, 0], [0, 0, 1], [0, 0, -1]], float
)


def interval(bounds, origins, directions):
    parallel = directions == 0
    times = np.divide(
        bounds[None] - origins[:, None],
        directions[:, None],
        out=np.zeros((len(origins), 2, 3)),
        where=~parallel[:, None],
    )
    low = np.where(parallel, -np.inf, times.min(axis=1))
    high = np.where(parallel, np.inf, times.max(axis=1))
    axis = low.argmax(axis=1)
    face = axis * 2 + (directions[np.arange(len(origins)), axis] < 0)
    face = np.where(axis == 2, 4 + (directions[:, 2] > 0), face)
    near = low[np.arange(len(origins)), axis]
    far = high.min(axis=1)
    missed = np.any(parallel & ((origins < bounds[0]) | (origins > bounds[1])), axis=1)
    return np.where(missed, np.inf, near), np.where(missed, -np.inf, far), face


def update(depth, owner, face, candidate_depth, candidate_owner, candidate_face):
    better = np.isfinite(candidate_depth) & (
        (candidate_depth < depth)
        | (
            (candidate_depth == depth)
            & (
                (owner < 0)
                | (candidate_owner < owner)
                | ((candidate_owner == owner) & (candidate_face < face))
            )
        )
    )
    depth[better] = candidate_depth[better]
    owner[better] = np.broadcast_to(candidate_owner, owner.shape)[better]
    face[better] = np.broadcast_to(candidate_face, face.shape)[better]


def boxes(product, chunk, origins, directions, maximum, work):
    depth = maximum.copy()
    owner = np.full(len(origins), -1, np.int64)
    face = np.full(len(origins), -1, np.int8)
    start = product.origin(chunk)
    half = product.pitch * product.frame[4] / 2
    for y, x in np.argwhere(product.tiers[chunk] > 0):
        centre = start + (np.array([x, y]) + 0.5) * product.pitch
        height = product.heights[product.tiers[chunk, y, x]]
        bounds = np.array([[*(centre - half), 0], [*(centre + half), height]])
        near, far, boundary = interval(bounds, origins, directions)
        candidate = np.where((near > 0.1) & (near <= far), near, np.inf)
        update(
            depth,
            owner,
            face,
            candidate,
            chunk * product.side**2 + y * product.side + x,
            boundary,
        )
        work["box_ray_tests"] += len(origins)
    depth[owner < 0] = np.inf
    return depth, owner, face


def mask_hits(product, chunk, points, descriptor):
    axis, _, coordinate, face, _ = descriptor
    origin, pitch = product.origin(chunk), product.pitch
    grid = (points[:, :2] - origin) / pitch
    cells = np.floor(grid).astype(np.int64)
    valid = ((cells >= 0) & (cells < product.side)).all(axis=1)
    cells = np.clip(cells, 0, product.side - 1)
    x, y = cells.T
    tier = product.tiers[chunk, y, x]
    height = product.heights[tier]
    valid &= tier > 0
    centre = origin + (cells + 0.5) * pitch
    half = product.frame[4] * pitch / 2
    for horizontal in range(2):
        if axis != horizontal:
            valid &= points[:, horizontal] >= centre[:, horizontal] - half
            valid &= points[:, horizontal] <= centre[:, horizontal] + half
    if axis == 2:
        if face == 4:
            valid &= height == coordinate
    else:
        valid &= (points[:, 2] >= 0) & (points[:, 2] <= height)
    owners = chunk * product.side**2 + y * product.side + x
    return valid, owners


def masks(product, chunk, origins, directions, maximum, work):
    depth = maximum.copy()
    owner = np.full(len(origins), -1, np.int64)
    face = np.full(len(origins), -1, np.int8)
    for descriptor in descriptors(product, chunk):
        axis, sign, coordinate, boundary, _ = descriptor
        divisor = directions[:, axis]
        time = np.divide(
            coordinate - origins[:, axis],
            divisor,
            out=np.full(len(origins), np.inf),
            where=divisor * sign < 0,
        )
        ids = np.flatnonzero((time > 0.1) & (time <= depth) & np.isfinite(time))
        work["plane_ray_tests"] += len(origins)
        if not len(ids):
            continue
        points = origins[ids] + time[ids, None] * directions[ids]
        hit, candidate_owner = mask_hits(product, chunk, points, descriptor)
        work["mask_queries"] += len(ids)
        candidate_depth = np.full(len(origins), np.inf)
        candidate_ids = np.full(len(origins), -1, np.int64)
        candidate_depth[ids[hit]] = time[ids[hit]]
        candidate_ids[ids[hit]] = candidate_owner[hit]
        update(depth, owner, face, candidate_depth, candidate_ids, boundary)
    depth[owner < 0] = np.inf
    return depth, owner, face


def trace(product, origins, directions, *, mode="masks", chunks=None):
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
    if (
        origins.shape != directions.shape
        or origins.ndim != 2
        or origins.shape[1] != 3
        or len(origins) > 4194304
        or not np.isfinite(origins).all()
        or not np.isfinite(directions).all()
        or np.any(np.linalg.norm(directions, axis=1) == 0)
    ):
        raise ValueError("bounded finite nonzero ray packet required")
    if mode not in {"masks", "boxes"}:
        raise ValueError("explicit mask or box consumer required")
    chunks = np.arange(len(product.chunks)) if chunks is None else np.asarray(chunks)
    if chunks.ndim != 1 or np.any(chunks < 0) or np.any(chunks >= len(product.chunks)):
        raise ValueError("valid selected patch IDs required")
    query = masks if mode == "masks" else boxes
    depth = np.full(len(origins), np.inf)
    owner = np.full(len(origins), -1, np.int64)
    face = np.full(len(origins), -1, np.int8)
    work = {
        "patch_box_ray_tests": 0,
        "box_ray_tests": 0,
        "plane_ray_tests": 0,
        "mask_queries": 0,
    }
    for chunk in chunks:
        near, far, _ = interval(product.bounds[chunk], origins, directions)
        ids = np.flatnonzero((np.maximum(near, 0.1) <= far) & (near <= depth))
        work["patch_box_ray_tests"] += len(origins)
        if not len(ids):
            continue
        t, o, f = query(product, chunk, origins[ids], directions[ids], depth[ids], work)
        current_t, current_o, current_f = depth[ids], owner[ids], face[ids]
        update(current_t, current_o, current_f, t, o, f)
        depth[ids], owner[ids], face[ids] = current_t, current_o, current_f
    normal = np.zeros((len(origins), 3))
    normal[owner >= 0] = NORMALS[face[owner >= 0]]
    return depth, owner, face, normal, work
