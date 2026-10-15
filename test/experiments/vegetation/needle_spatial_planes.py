"""Bounded greedy face-plane patches with a complete original-geometry remainder."""

import numpy as np


def plane_patches(source, epsilon_m, candidates=128, max_charts=64):
    if not np.isfinite(epsilon_m) or epsilon_m <= 0:
        raise ValueError("finite positive object-space displacement required")
    if type(candidates) is not int or not 1 <= candidates <= 512:
        raise ValueError("one to 512 candidate planes required")
    if type(max_charts) is not int or not 0 <= max_charts <= 256:
        raise ValueError("zero to 256 charts required")
    triangles = source["vertices"][source["faces"]]
    vectors = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    double_area = np.linalg.norm(vectors, axis=1)
    eligible = np.array([kind == "leaf" for kind, _ in source["ids"]])[
        source["organ_faces"]
    ]
    eligible &= double_area > 1e-20
    weights = np.where(eligible, double_area / 2, 0)
    if weights.sum() == 0:
        return [], np.arange(len(triangles))
    quantiles = (np.arange(candidates) + 0.5) / candidates * weights.sum()
    seeds = np.unique(np.searchsorted(np.cumsum(weights), quantiles))
    normals = vectors[seeds] / double_area[seeds, None]
    offsets = np.einsum("ij,ij->i", normals, triangles[seeds].mean(axis=1))
    coverage = np.zeros((len(seeds), len(triangles)), bool)
    benefit = np.zeros_like(coverage, dtype=np.float32)
    for first in range(0, len(seeds), 16):
        last = min(first + 16, len(seeds))
        z = np.einsum("fij,pj->pfi", triangles, normals[first:last])
        valid = np.max(abs(z - offsets[first:last, None, None]), axis=-1) <= epsilon_m
        coverage[first:last] = valid & eligible
        benefit[first:last] = (
            abs(vectors @ normals[first:last].T).T / 2 * coverage[first:last]
        )
    scores = benefit.sum(axis=1, dtype=np.float64)
    remaining = np.ones(len(triangles), bool)
    patches = []
    for _ in range(max_charts):
        index = int(np.argmax(scores))
        if scores[index] <= 0:
            break
        members = np.flatnonzero(coverage[index] & remaining)
        if len(members) < 4:
            scores[index] = -np.inf
            continue
        projected = triangles[members] @ normals[index]
        offset = (projected.min() + projected.max()) / 2
        displacement = float(abs(projected - offset).max())
        assert displacement <= epsilon_m * (1 + 1e-10)
        patches.append(
            {
                "faces": members,
                "normal": normals[index],
                "offset_m": float(offset),
                "displacement_m": displacement,
            }
        )
        remaining[members] = False
        scores -= benefit[:, members].sum(axis=1, dtype=np.float64)
        scores[index] = -np.inf
    return patches, np.flatnonzero(remaining)


def face_subset(source, indices):
    selected = source["faces"][indices]
    vertices, inverse = np.unique(selected, return_inverse=True)
    subset = dict(
        source, vertices=source["vertices"][vertices], faces=inverse.reshape(-1, 3)
    )
    for name in ("normals", "colour", "roughness", "organ_faces"):
        subset[name] = source[name][indices]
    return subset
