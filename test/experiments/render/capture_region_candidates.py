"""Bounded projected-centroid blocker proposals; exact region proof remains separate.

All triangles must lie in front of this proposal camera. A missed/incorrect
proposal retains the target; this module never certifies opacity or coverage.
"""

import numpy as np
import shapely
from shapely import STRtree


def propose(triangles, eye, basis, *, pair_budget=262144, total_pair_budget=8388608):
    if (
        not isinstance(triangles, np.ndarray)
        or triangles.dtype != np.float64
        or triangles.ndim != 3
        or triangles.shape[1:] != (3, 3)
        or len(triangles) > 131072
        or not np.isfinite(triangles).all()
        or np.shape(eye) != (3,)
        or np.shape(basis) != (3, 3)
        or not np.isfinite(eye).all()
        or not np.isfinite(basis).all()
        or type(pair_budget) is not int
        or pair_budget < 1
        or type(total_pair_budget) is not int
        or total_pair_budget < 1
    ):
        raise ValueError("bounded finite projected triangle input required")
    answer = np.full(len(triangles), -1, np.int64)
    report = {"targets": len(answer), "candidate_pairs": 0, "maximum_batch_pairs": 0}
    if not len(answer):
        return answer, report
    local = (triangles - eye) @ basis.T
    if (local[:, :, 2] <= 0.1).any():
        return answer, dict(report, reason="geometry_crosses_proposal_near_plane")
    xy = local[:, :, :2] / local[:, :, 2:]
    centre = local.mean(axis=1)
    query = centre[:, :2] / centre[:, 2:]
    normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    front = np.einsum("ij,ij->i", normal, triangles[:, 0] - eye) < 0
    ids = np.flatnonzero(front)
    if not len(ids):
        return answer, report
    if len(ids) > pair_budget:
        return answer, dict(report, reason="candidate_pair_budget_too_small")
    low, high = xy[ids].min(axis=1), xy[ids].max(axis=1)
    tree = STRtree(shapely.box(low[:, 0], low[:, 1], high[:, 0], high[:, 1]))
    # Even an all-overlapping box query cannot allocate more than pair_budget pairs.
    batch = min(128, pair_budget // len(ids))
    for begin in range(0, len(answer), batch):
        maximum_pairs = min(batch, len(answer) - begin) * len(ids)
        if report["candidate_pairs"] + maximum_pairs > total_pair_budget:
            report.update(
                reason="total_candidate_budget_exhausted",
                unresolved_targets=len(answer) - begin,
            )
            break
        q = query[begin : begin + batch]
        rays, faces = tree.query(shapely.points(q)).astype(np.int64, copy=False)
        faces = ids[faces]
        count = len(faces)
        report["candidate_pairs"] += count
        report["maximum_batch_pairs"] = max(report["maximum_batch_pairs"], count)
        if not count:
            continue
        t = xy[faces]
        a, b = t[:, 0] - t[:, 2], t[:, 1] - t[:, 2]
        p = q[rays] - t[:, 2]
        det = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
        valid = det != 0
        u = np.divide(
            p[:, 0] * b[:, 1] - p[:, 1] * b[:, 0],
            det,
            out=np.full(count, -1.0),
            where=valid,
        )
        v = np.divide(
            a[:, 0] * p[:, 1] - a[:, 1] * p[:, 0],
            det,
            out=np.full(count, -1.0),
            where=valid,
        )
        w = 1 - u - v
        inverse_z = np.sum(np.c_[u, v, w] / local[faces, :, 2], axis=1)
        inside = valid & (u >= 0) & (v >= 0) & (w >= 0)
        inside &= (faces != begin + rays) & (inverse_z > 1 / centre[begin + rays, 2])
        selected = np.flatnonzero(inside)
        order = np.lexsort((faces[selected], -inverse_z[selected], rays[selected]))
        selected = selected[order]
        hit_rays = rays[selected]
        first = (
            np.r_[True, hit_rays[1:] != hit_rays[:-1]]
            if len(selected)
            else np.empty(0, bool)
        )
        answer[begin + hit_rays[first]] = faces[selected[first]]
    report["proposal_array_bytes"] = sum(
        x.nbytes for x in (answer, local, xy, centre, query, normal, ids, low, high)
    )
    report["scope"] = (
        "centroid proposals only; bound limits query-pair arrays, not GEOS/index/total allocator memory"
    )
    return answer, report
