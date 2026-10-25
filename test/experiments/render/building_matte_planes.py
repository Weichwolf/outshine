"""Finite oriented source-plane cover; no triangulation, normal means or Hough volume."""

import numpy as np

NORMAL_LIMIT = 32
PLANE_LIMIT = 128


def source_geometry(patches):
    corners, normals, areas = [], [], []
    for patch in patches:
        surface = patch.geometry
        low_x, low_y, high_x, high_y = surface.domain.bounds
        q = np.array([[x, y] for x in (low_x, high_x) for y in (low_y, high_y)])
        corners.append(surface.origin + q @ surface.axes)
        normals.append(surface.normal)
        areas.append(surface.domain.area * np.linalg.norm(np.cross(*surface.axes)))
    corners, normals, areas = map(np.asarray, (corners, normals, areas))
    if (
        not 0 < len(patches) <= 65536
        or not all(np.isfinite(a).all() for a in (corners, normals, areas))
        or np.any(areas <= 0)
        or np.any(abs(np.linalg.norm(normals, axis=1) - 1) > 1e-6)
    ):
        raise ValueError(
            "bounded positive source domains with individual unit normals required"
        )
    return corners, normals / np.linalg.norm(normals, axis=1)[:, None], areas


def representative_normals(normals, areas):
    best = np.full(len(normals), -1.0)
    selected = []
    for _ in range(min(NORMAL_LIMIT, len(normals))):
        score = areas * (1 - best)
        at = int(np.argmax(score))
        if score[at] <= 0:
            break
        selected.append(at)
        best = np.maximum(best, np.clip(normals @ normals[at], -1, 1))
        best[selected] = 1
    return np.asarray(selected, int)


def candidate_planes(corners, normals, areas, epsilon):
    candidates = []
    representatives = representative_normals(normals, areas)
    comparisons = 0
    for at in representatives:
        normal = normals[at]
        height = corners @ normal
        low, high = height.min(axis=1), height.max(axis=1)
        offsets = np.unique(np.rint((low + high) / (2 * epsilon))) * epsilon
        cosine = normals @ normal
        projected = areas * np.maximum(cosine, 0)
        for offset in offsets:
            error = abs(height - offset).max(axis=1)
            margin = (
                32
                * np.finfo(float).eps
                * (abs(height).max(axis=1) + abs(offset) + epsilon)
            )
            valid = (error + margin <= epsilon) & (cosine >= 0.5)
            comparisons += len(corners)
            if not valid.any():
                continue
            miss = (
                (high - low <= 2 * epsilon)
                & (offset >= high - 2 * epsilon)
                & (offset < high - epsilon)
                & (cosine >= 0.5)
            )
            included, missed = np.flatnonzero(valid), np.flatnonzero(miss)
            candidates.append(
                (
                    normal,
                    float(offset),
                    included,
                    projected[included],
                    missed,
                    projected[missed],
                )
            )
            if len(candidates) > 8192:
                raise ValueError("bounded finite plane candidate budget exhausted")
    return candidates, {
        "representative_normals": len(representatives),
        "candidate_normal_dot_rows": len(representatives) * len(normals),
        "representative_normal_dot_rows": len(representatives) * len(normals),
        "source_corner_height_dot_rows": len(representatives) * len(normals) * 4,
        "candidate_patch_comparisons": comparisons,
        "candidate_planes": len(candidates),
        "known_candidate_index_weight_bytes": sum(
            v.nbytes for c in candidates for v in c[2:]
        ),
    }


def greedy_planes(candidates, count):
    remaining = np.ones(count, bool)
    assignment = np.full(count, -1, np.int32)
    selected = []
    inspected = 0
    for _ in range(PLANE_LIMIT):
        scores = []
        for _, _, valid, weight, miss, penalty in candidates:
            inspected += len(valid) + len(miss)
            scores.append(
                weight[remaining[valid]].sum() - 10 * penalty[remaining[miss]].sum()
            )
        if not scores:
            break
        at = int(np.argmax(scores))
        if scores[at] <= 0:
            break
        covered = candidates[at][2]
        covered = covered[remaining[covered]]
        assignment[covered] = len(selected)
        remaining[covered] = False
        selected.append(at)
        if not remaining.any():
            break
    return assignment, selected, inspected


def cover_domains(patches, bounds):
    corners, normals, areas = source_geometry(patches)
    bounds = np.asarray(bounds, float)
    epsilon = float(np.linalg.norm(bounds[1] - bounds[0]) / 64)
    if not np.isfinite(epsilon) or epsilon <= 0:
        raise ValueError("positive finite root displacement scale required")
    candidates, work = candidate_planes(corners, normals, areas, epsilon)
    assignment, chosen, inspected = greedy_planes(candidates, len(patches))
    sources = np.array([p.geometry.source for p in patches], np.uint32)
    declined = np.unique(sources[assignment < 0])
    assignment[np.isin(sources, declined)] = -1
    kept = assignment >= 0
    used, remap = np.unique(assignment[kept], return_inverse=True)
    assignment[kept] = remap
    planes = [candidates[chosen[int(i)]] for i in used]
    plane_normal = np.array([p[0] for p in planes]).reshape(-1, 3)
    plane_offset = np.array([p[1] for p in planes])
    error = abs(
        np.einsum("nki,ni->nk", corners[kept], plane_normal[assignment[kept]])
        - plane_offset[assignment[kept], None]
    )
    if np.any(error > epsilon):
        raise ValueError("source-domain bounding corners exceed declared displacement")
    result = {
        "plane_normal": plane_normal,
        "plane_offset": plane_offset,
        "patch_planes": assignment,
        "source_fallback": declined,
        "epsilon": np.array(epsilon),
    }
    work.update(
        {
            "source_patches": len(patches),
            "selected_planes": len(planes),
            "selected_before_whole_source_fallback": len(chosen),
            "assigned_patches": int(kept.sum()),
            "original_source_fallbacks": declined.tolist(),
            "covered_sources": len(np.unique(sources[kept])),
            "maximum_domain_corner_displacement_m": float(error.max(initial=0)),
            "declared_displacement_m": epsilon,
            "greedy_index_entries_examined": inspected,
            "known_source_corner_normal_area_bytes": sum(
                a.nbytes for a in (corners, normals, areas)
            ),
        }
    )
    return result, work
