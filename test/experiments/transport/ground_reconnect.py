"""Bounded local soil-edge exchanges; fixed heights, traffic and outer edges."""

import time

import numpy as np
import shapely

from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import _face_gradients


def _cross(a, b):
    return a[..., 0] * b[..., 1] - a[..., 1] * b[..., 0]


def _pairs(faces, protected):
    edges = np.sort(np.vstack((faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]])), axis=1)
    _, labels, counts = np.unique(edges, axis=0, return_inverse=True, return_counts=True)
    if np.any(counts > 2):
        raise ValueError("manifold edge-matched input required")
    order = np.argsort(labels, kind="stable")
    starts = np.r_[0, np.cumsum(counts)]
    used = np.flatnonzero(counts == 2)
    owners = np.tile(np.arange(len(faces)), 3)
    parents = np.c_[owners[order[starts[used]]], owners[order[starts[used] + 1]]]
    allowed = np.all(parents >= protected, axis=1)
    edge = edges[order[starts[used]]][allowed]
    parents = parents[allowed]
    old = faces[parents]
    other = old[(old != edge[:, None, 0:1]) & (old != edge[:, None, 1:2])].reshape(-1, 2)
    return parents, np.c_[edge, other]


def _minimum_angle(xy):
    u, v = np.roll(xy, -1, axis=-2) - xy, np.roll(xy, 1, axis=-2) - xy
    return np.arctan2(abs(_cross(u, v)), np.einsum("...i,...i->...", u, v)).min(axis=(-1, -2))


def _proposals(mesh, protected, grade, method):
    parents, quad = _pairs(mesh.triangles, protected)
    xy = mesh.vertices[quad, :2]
    a, b, c, d = np.moveaxis(xy, 1, 0)
    margin = np.maximum(np.ptp(xy, axis=1).max(axis=1) ** 2 * 1e-12, 1e-12)
    s, t = _cross(b - a, c - a), _cross(b - a, d - a)
    u, v = _cross(d - c, a - c), _cross(d - c, b - c)
    convex = ((s > margin) & (t < -margin) | (s < -margin) & (t > margin))
    convex &= ((u > margin) & (v < -margin) | (u < -margin) & (v > margin))
    parents, quad, xy = parents[convex], quad[convex], xy[convex]
    faces = quad[:, [[2, 3, 0], [3, 2, 1]]].copy()
    q = mesh.vertices[faces, :2]
    reverse = _cross(q[:, :, 1] - q[:, :, 0], q[:, :, 2] - q[:, :, 0]) < 0
    faces[reverse] = faces[reverse][:, [0, 2, 1]]
    old_xy = mesh.vertices[mesh.triangles[parents], :2]
    gain = _minimum_angle(mesh.vertices[faces, :2]) - _minimum_angle(old_xy)
    gradients = _face_gradients(mesh.vertices[:, :2], faces.reshape(-1, 3), mesh.vertices[:, 2]).reshape(-1, 2, 2)
    valid = (np.linalg.norm(gradients, axis=2) <= grade * 1.0000001).all(axis=1)
    if method == "delaunay" and len(quad):
        edges, owner = shapely.get_parts(shapely.delaunay_triangles(shapely.multipoints(xy), only_edges=True), return_index=True)
        positions = shapely.get_coordinates(edges).reshape(-1, 2, 2)
        selected = ((positions[:, 0] == xy[owner, 2]).all(axis=1) & (positions[:, 1] == xy[owner, 3]).all(axis=1))
        selected |= ((positions[:, 0] == xy[owner, 3]).all(axis=1) & (positions[:, 1] == xy[owner, 2]).all(axis=1))
        chosen = np.zeros(len(quad), dtype=bool)
        chosen[owner[selected]] = True
        valid &= chosen & (gain > 1e-10)
    old_g = _face_gradients(mesh.vertices[:, :2], mesh.triangles[parents].reshape(-1, 3), mesh.vertices[:, 2]).reshape(-1, 2, 2)
    crease_gain = np.linalg.norm(old_g[:, 0] - old_g[:, 1], axis=1) - np.linalg.norm(gradients[:, 0] - gradients[:, 1], axis=1)
    return parents[valid], faces[valid], crease_gain[valid]


def _patch_heights(points, vertices):
    xy = vertices[..., :2]
    a, b = xy[:, :, 1] - xy[:, :, 0], xy[:, :, 2] - xy[:, :, 0]
    d = points[:, None] - xy[:, :, 0, None]
    determinant = _cross(a, b)
    s, t = _cross(d, b[:, :, None]) / determinant[:, :, None], _cross(a[:, :, None], d) / determinant[:, :, None]
    inside = (s >= -1e-8) & (t >= -1e-8) & (s + t <= 1 + 1e-8)
    if not inside.any(axis=1).all():
        raise ValueError("local exchange does not cover its original patch")
    z = vertices[..., 2]
    height = z[:, :, 0, None] + s * (z[:, :, 1] - z[:, :, 0])[:, :, None] + t * (z[:, :, 2] - z[:, :, 0])[:, :, None]
    return np.where(inside[:, 0], height[:, 0], height[:, 1])


def _probe_gain(mesh, parents, faces, target):
    bary = np.array([[i, j, 4 - i - j] for i in range(5) for j in range(5 - i)], dtype=float) / 4
    old, new = mesh.vertices[mesh.triangles[parents]], mesh.vertices[faces]
    queries = np.einsum("sj,kpji->kpsi", bary, np.concatenate((old, new), axis=1)[..., :2]).reshape(len(parents), -1, 2)
    truth = np.asarray(target(queries.reshape(-1, 2)), dtype=float)
    if truth.shape != (queries.shape[0] * queries.shape[1],):
        raise ValueError("one DEM height per local probe required")
    truth = truth.reshape(queries.shape[:2])
    before = abs(truth - _patch_heights(queries, old))
    after = abs(truth - _patch_heights(queries, new))
    gain = np.mean(before * before - after * after, axis=1)
    valid = np.isfinite(truth).all(axis=1) & (after.max(axis=1) <= before.max(axis=1) + 1e-8)
    return valid & (gain > 1e-8), np.where(valid, gain, 0.), queries.shape[0] * queries.shape[1]


def reconnect_ground(mesh, target, *, protected_faces=0, method="delaunay", grade=2 / 3,
                     max_passes=8, max_quality_queries=1000000, max_faces=65536,
                     max_vertices=65536, time_limit_s=12.):
    """Exchange only soil/soil edges of strictly convex local pairs.

    GEOS-guided Delaunay and data-dependent fits use the same local DEM checks.
    This is bounded heuristic work, not a complete CDT or global error bound.
    Input must be a conforming nonoverlapping XY coverage. All XYZ values stay
    fixed. Parent-root LOD certificates do not survive swaps. Time is checked
    between bounded phases, and a phase can overrun; this is offline work.
    """
    began = time.perf_counter()
    if method not in ("delaunay", "fit") or not np.isfinite(grade) or grade <= 0:
        raise ValueError("known exchange method and finite positive grade required")
    if any(type(v) is not int or v < 0 for v in (protected_faces, max_passes, max_quality_queries, max_faces, max_vertices)):
        raise ValueError("nonnegative integer work caps required")
    if not np.isfinite(time_limit_s) or time_limit_s < 0:
        raise ValueError("finite nonnegative cooperative time limit required")
    if protected_faces > len(mesh.triangles) or len(mesh.triangles) > max_faces or len(mesh.vertices) > max_vertices:
        raise ValueError("input/protected prefix exceeds the geometry caps")
    g = _face_gradients(mesh.vertices[:, :2], mesh.triangles[protected_faces:], mesh.vertices[:, 2])
    if len(g) and np.linalg.norm(g, axis=1).max() > grade * 1.0000001:
        raise ValueError("input soil exceeds whole-face grade")
    steps, probes, swaps, reason = [], 0, 0, "pass_budget_exhausted"
    for _ in range(max_passes):
        if time.perf_counter() - began >= time_limit_s:
            reason = "cooperative_time_budget_exhausted"
            break
        capacity = (max_quality_queries - probes) // 60
        if not capacity:
            reason = "quality_budget_exhausted"
            break
        parents, faces, crease = _proposals(mesh, protected_faces, grade, method)
        if time.perf_counter() - began >= time_limit_s:
            reason = "cooperative_time_budget_exhausted"
            break
        rank = np.lexsort((parents[:, 0], -crease))[:capacity]
        parents, faces = parents[rank], faces[rank]
        if not len(parents):
            reason = "no_admissible_exchanges"
            break
        valid, gain, count = _probe_gain(mesh, parents, faces, target)
        probes += count
        chosen, occupied = [], set()
        for i in np.lexsort((parents[:, 0], -gain)):
            if valid[i] and not any(int(p) in occupied for p in parents[i]):
                chosen.append(i)
                occupied.update(map(int, parents[i]))
        if not chosen:
            reason = "no_local_probe_gain"
            break
        changed = mesh.triangles.copy()
        changed[parents[chosen].reshape(-1)] = faces[chosen].reshape(-1, 3)
        mesh = Mesh(mesh.vertices, changed)
        swaps += len(chosen)
        steps.append(dict(candidates=len(parents), exchanged=len(chosen), quality_queries=count))
    return mesh, dict(method=method, exchanges=swaps, quality_queries=probes, steps=steps, reason=reason,
                      reconnect_ms=(time.perf_counter() - began) * 1000, old_vertices_fixed=True,
                      protected_faces=protected_faces, global_dem_bound=False, root_lod_certificate=False,
                      time_limit_s=time_limit_s, time_limit_cooperative=True)
