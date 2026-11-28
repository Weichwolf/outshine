"""Representative leaf sheets fitted to the geometry of each shoot group."""

from dataclasses import replace

import numpy as np

KITE_FACES = np.array([[0, 1, 2], [0, 2, 3]], dtype=np.int32)


def leaf_group_ranges(rows, step, *, merge_shoots):
    axes = rows["leaf_axis_ids"]
    order = np.arange(len(axes))
    if merge_shoots:
        parents = rows["axis_parent_ids"][axes]
        axes = np.where(parents >= 0, parents, axes)
        order = np.argsort(axes, kind="stable")
    _, starts, counts = np.unique(axes[order], return_index=True, return_counts=True)
    first = np.concatenate(
        [np.arange(i, i + n, step) for i, n in zip(starts, counts, strict=True)]
    )
    ends = starts + counts
    stop = np.minimum(
        first + step, ends[np.searchsorted(starts, first, side="right") - 1]
    )
    return order, first, stop


def blade_kite(blade):
    blade = replace(blade, petiole=0, base_fill=0)
    length = np.array([0.0, 0.5, 1.0, 0.5])
    width = np.array([0.0, 1.0, 0.0, -1.0])
    vertices = blade.evaluate(length, width, np.where(width < 0, -1, 1))[0]
    corners = vertices[KITE_FACES]
    area = 0.5 * np.linalg.norm(
        np.cross(corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0]), axis=1
    )
    centre = area @ corners.mean(axis=1) / area.sum()
    return vertices - centre, area.sum(), centre


def cluster_leaves(points, areas, normals, indices, valid):
    indexed = points[indices]
    weights = areas[indices] * valid
    mean = np.einsum("gki,gk->gi", indexed, weights) / weights.sum(axis=1)[:, None]
    centred = indexed - mean[:, None, :]
    # Split where the leaf centres spread most, even when a group spans several shoots.
    covariance = np.einsum("gki,gkj,gk->gij", centred, centred, weights)
    major = np.linalg.eigh(covariance)[1][:, :, -1]
    positive = np.einsum("gki,gi->gk", centred, major) > 0
    for side in (valid & positive, valid & ~positive):
        present = side.any(axis=1)
        positions = indexed[present]
        mass = weights[present] * side[present]
        total = mass.sum(axis=1)
        centre = np.einsum("gki,gk->gi", positions, mass) / total[:, None]
        distance = np.linalg.norm(positions - centre[:, None, :], axis=2)
        distance /= np.maximum(distance.max(axis=1, keepdims=True), 1e-8)
        directions = normals[indices[present]]
        tensor = np.einsum(
            "gki,gkj,gk->gij", directions, directions, mass / total[:, None]
        )
        # Prefer a central leaf whose orientation is representative of this half.
        coherence = np.einsum("gki,gij,gkj->gk", directions, tensor, directions)
        choice = np.argmax(
            np.where(side[present], coherence - 0.65 * distance, -np.inf), axis=1
        )
        yield indices[present][np.arange(len(choice)), choice], total, centre


def grouped_blades(rows, blades, frames, areas, step, *, merge_shoots):
    if not len(frames):
        return np.empty((0, 3)), np.empty((0, 3), dtype=np.int32)
    kites, kite_areas, base_offsets = map(
        np.asarray, zip(*(blade_kite(b) for b in blades))
    )
    variants = rows["leaf_variants"]
    leaf_centres = rows["leaf_origins"] + np.einsum(
        "nij,nj->ni", frames, base_offsets[variants]
    )
    order, first, stop = leaf_group_ranges(rows, step, merge_shoots=merge_shoots)
    offsets = first[:, None] + np.arange(step)
    valid = offsets < stop[:, None]
    indices = order[np.where(valid, offsets, first[:, None])]
    groups = cluster_leaves(
        leaf_centres, areas[variants], frames[:, :, 2], indices, valid
    )
    selected, total, centres = map(np.concatenate, zip(*groups))
    variant = variants[selected]
    scale = np.sqrt(total / kite_areas[variant])
    vertices = (
        np.einsum("nkj,nij->nki", kites[variant], frames[selected])
        * scale[:, None, None]
    )
    vertices += centres[:, None, :]
    faces = KITE_FACES + (4 * np.arange(len(vertices), dtype=np.int32))[:, None, None]
    return vertices.reshape(-1, 3), faces.reshape(-1, 3)
