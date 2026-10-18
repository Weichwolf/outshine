"""Bounded window planes from existing finite views; no physical organ expansion."""

import numpy as np
from foliage_selection import directions


def select_views(windows, count, *, eye=None, direction=None):
    if (eye is None) == (direction is None):
        raise ValueError("one perspective eye or distant light direction required")
    rays = (
        np.asarray(eye) - windows["origins"]
        if eye is not None
        else np.broadcast_to(direction, windows["origins"].shape)
    )
    if not np.isfinite(rays).all() or np.any(np.linalg.norm(rays, axis=1) == 0):
        raise ValueError("finite nonzero source-local view directions required")
    local = np.einsum("ni,nij->nj", rays, windows["frames"])
    return np.argmax(local @ directions(count).T, axis=1)


def planes(windows, bank, selected, indices, family, *, max_windows=80000):
    count = len(indices)
    if not 0 < count <= max_windows:
        raise ValueError("nonempty bounded window demand required before planes")
    if len(bank) != 64 or np.any((selected < 0) | (selected >= len(bank))):
        raise ValueError(
            "complete finite view catalog and valid selected views required"
        )
    quads, uvs = [], []
    for index, (image, (basis, lo, hi, _)) in enumerate(bank):
        if image.shape != (32, 32, 9) or not np.isfinite(image).all():
            raise ValueError("finite complete physical window channels required")
        offset = image[..., 8].sum() / max(image[..., 3].sum(), 1e-20)
        xy = np.array(((lo[0], lo[1]), (hi[0], lo[1]), (hi[0], hi[1]), (lo[0], hi[1])))
        quads.append(np.column_stack((xy, np.full(4, offset))) @ basis.T)
        uv = np.array(((0, 0), (1, 0), (1, 1), (0, 1)), dtype=float)
        uvs.append((uv * 32 + (index % 8 * 40 + 4, index // 8 * 40 + 4)) / 320)
    picked = selected[indices]
    vertices = np.einsum(
        "nij,nvj->nvi", windows["frames"][indices], np.array(quads)[picked]
    )
    vertices += windows["origins"][indices, None]
    faces = (
        np.arange(count)[:, None, None] * 4 + np.array(((0, 1, 2), (0, 2, 3)))
    ).reshape(-1, 3)
    return {
        "vertices": vertices.reshape(-1, 3),
        "faces": faces.astype(np.int32),
        "uv": np.array(uvs)[picked].reshape(-1, 2),
        "weights": np.repeat(windows["weights"][indices], 2),
        "source_window_indices": np.asarray(indices),
        "first_needle_ids": windows["first_needle_ids"][indices],
        "family": family,
    }
