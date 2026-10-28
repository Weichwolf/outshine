"""Bounded conforming terrain details with fixed old vertices and road contacts."""

import time

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import _face_gradients

METHODS = ("midpoint", "centroid", "quartiles")


def _edge_probes(mesh, candidates, parameters, maximum_candidates):
    count = len(candidates["xy"])
    if count * len(parameters) > maximum_candidates:
        raise ValueError("refinement_probe_budget")
    result = {
        key: np.repeat(candidates[key], len(parameters), axis=0)
        for key in ("parents", "fixed", "vertices")
    }
    fraction = np.tile(parameters, count)
    edge = mesh.vertices[result["vertices"]]
    result.update(
        parameter=fraction,
        xy=(1 - fraction[:, None]) * edge[:, 0, :2]
        + fraction[:, None] * edge[:, 1, :2],
        height=(1 - fraction) * edge[:, 0, 2] + fraction * edge[:, 1, 2],
    )
    return result


def _intervals(gradient, coefficient, grade):
    u, c = coefficient / grade, gradient / grade
    length = np.hypot(u[..., 0], u[..., 1])
    along = np.einsum("...i,...i->...", u, c) / length
    across = (u[..., 0] * c[..., 1] - u[..., 1] * c[..., 0]) / length
    discriminant = 1.0 - across * across
    radius = np.sqrt(np.maximum(discriminant, 0.0))
    low, high = (-along - radius) / length, (-along + radius) / length
    possible = np.all(discriminant >= -1e-12, axis=1)
    low, high = low.max(axis=1), high.min(axis=1)
    possible &= low <= high + 1e-10
    # The accepted input plane is the fallback, including its numerical tolerance.
    return np.minimum(low, 0.0), np.maximum(high, 0.0), possible


def _candidates(mesh, protected, method, *, maximum_candidates=65536):
    faces, p = mesh.triangles, mesh.vertices
    if method == "centroid":
        parents = np.arange(protected, len(faces))[:, None]
        ids = faces[parents[:, 0]]
        q = p[ids, :2].mean(axis=1)
        fixed = np.stack((p[ids, :2], np.roll(p[ids, :2], -1, axis=1)), axis=2)
        return {
            "parents": parents,
            "xy": q,
            "height": p[ids, 2].mean(axis=1),
            "fixed": fixed,
            "vertices": ids,
        }
    raw = np.vstack((faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]))
    edges, labels, counts = np.unique(
        np.sort(raw, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    owner = np.tile(np.arange(len(faces)), 3)
    order = np.argsort(labels, kind="stable")
    starts = np.r_[0, np.cumsum(counts)]
    chosen = np.flatnonzero(counts == 2)
    parents = np.c_[owner[order[starts[chosen]]], owner[order[starts[chosen] + 1]]]
    retained = np.all(parents >= protected, axis=1)
    parents, edges = parents[retained], edges[chosen[retained]]
    ids = faces[parents]
    mask = (ids != edges[:, None, 0:1]) & (ids != edges[:, None, 1:2])
    opposite = ids[mask].reshape(-1, 2)
    a, b, c = p[edges[:, 0], :2], p[edges[:, 1], :2], p[opposite, :2]
    fixed = np.stack(
        (
            np.stack((np.broadcast_to(a[:, None], c.shape), c), axis=2),
            np.stack((c, np.broadcast_to(b[:, None], c.shape)), axis=2),
        ),
        axis=2,
    ).reshape(-1, 4, 2, 2)
    result = {
        "parents": parents,
        "xy": (a + b) * 0.5,
        "height": p[edges, 2].mean(axis=1),
        "fixed": fixed,
        "vertices": edges,
    }
    if method == "midpoint":
        return result
    parameters = np.array([0.25, 0.5, 0.75])
    return _edge_probes(mesh, result, parameters, maximum_candidates)


def _rank(mesh, candidates, target, grade, error, minimum_gain):
    q, fixed, parents = candidates["xy"], candidates["fixed"], candidates["parents"]
    u, v = fixed[:, :, 1] - fixed[:, :, 0], q[:, None] - fixed[:, :, 0]
    determinant = u[..., 0] * v[..., 1] - u[..., 1] * v[..., 0]
    valid = np.all(determinant != 0, axis=1)
    safe = np.where(determinant == 0, 1.0, determinant)
    coefficient = np.stack((-u[..., 1], u[..., 0]), axis=-1) / safe[..., None]
    gradient = _face_gradients(
        mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2]
    )
    repeats = fixed.shape[1] // parents.shape[1]
    selected_gradient = np.repeat(gradient[parents], repeats, axis=1)
    lower, upper, possible = _intervals(selected_gradient, coefficient, grade)
    desired = np.asarray(target(q), dtype=float)
    if desired.shape != (len(q),):
        raise ValueError("one target height per candidate required")
    known = np.isfinite(desired)
    change = np.where(known, desired - candidates["height"], 0.0)
    delta = np.clip(change, lower, upper)
    eligible = (
        valid & possible & known & (abs(change) > error) & (abs(delta) >= minimum_gain)
    )
    p = mesh.vertices[mesh.triangles, :2]
    a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    area = abs(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]) * 0.5
    score = abs(delta) * area[parents].sum(axis=1)
    rank = np.lexsort((np.arange(len(q)), -score))
    report = {
        "candidates": len(q),
        "missing_targets": int(np.count_nonzero(~known)),
        "grade_limited": int(
            np.count_nonzero(known & (abs(change) > error) & ~eligible)
        ),
    }
    return rank[eligible[rank]], delta, report


def _split(mesh, roots, candidates, selected, delta, method):
    vertices = np.vstack(
        (
            mesh.vertices,
            np.c_[
                candidates["xy"][selected],
                candidates["height"][selected] + delta[selected],
            ],
        )
    )
    changed, children, parent_ids = set(), [], []
    for new_vertex, candidate in enumerate(selected, len(mesh.vertices)):
        for parent in candidates["parents"][candidate]:
            face = mesh.triangles[parent].tolist()
            changed.add(int(parent))
            if method == "centroid":
                rows = [(face[i], face[(i + 1) % 3], new_vertex) for i in range(3)]
            else:
                endpoints = set(map(int, candidates["vertices"][candidate]))
                first = next(
                    i for i in range(3) if {face[i], face[(i + 1) % 3]} == endpoints
                )
                a, b, c = face[first], face[(first + 1) % 3], face[(first + 2) % 3]
                rows = [(a, new_vertex, c), (new_vertex, b, c)]
            children.extend(rows)
            parent_ids.extend([roots[parent]] * len(rows))
    kept = np.ones(len(mesh.triangles), dtype=bool)
    kept[list(changed)] = False
    faces = np.vstack((mesh.triangles[kept], np.asarray(children, dtype=np.uint32)))
    return Mesh(vertices, faces), np.r_[roots[kept], parent_ids]


def _quality_delta(mesh, candidates, selected, delta, target, method):
    bary = (
        np.array(
            [[i, j, 8 - i - j] for i in range(9) for j in range(9 - i)], dtype=float
        )
        / 8
    )
    parents = candidates["parents"][selected]
    face_ids = mesh.triangles[parents]
    vertices = mesh.vertices[face_ids]
    points = np.einsum("sj,kpji->kpsi", bary, vertices)
    truth = np.asarray(target(points[..., :2].reshape(-1, 2)), dtype=float).reshape(
        points.shape[:3]
    )
    residual = truth - points[..., 2]
    known = np.isfinite(truth).all(axis=(1, 2))
    maximum = np.max(np.where(np.isfinite(residual), abs(residual), 0.0), axis=(1, 2))
    if method == "centroid":
        weight = np.broadcast_to(3 * bary.min(axis=1), residual.shape)
    else:
        edges = candidates["vertices"][selected]
        a = np.einsum("sj,kpj->kps", bary, face_ids == edges[:, None, 0:1])
        b = np.einsum("sj,kpj->kps", bary, face_ids == edges[:, None, 1:2])
        if "parameter" in candidates:
            fraction = candidates["parameter"][selected, None, None]
            weight = np.minimum(a / (1 - fraction), b / fraction)
        else:
            weight = 2 * np.minimum(a, b)
    active = weight > 0
    denominator = np.where(active, weight, 1.0)
    lower = np.max(
        np.where(active, (residual - maximum[:, None, None]) / denominator, -np.inf),
        axis=(1, 2),
    )
    upper = np.min(
        np.where(active, (residual + maximum[:, None, None]) / denominator, np.inf),
        axis=(1, 2),
    )
    checked = np.clip(delta[selected], np.minimum(lower, 0.0), np.maximum(upper, 0.0))
    checked[~known] = 0.0
    return checked, len(selected) * parents.shape[1] * len(bary)


def vertical_lod_difference(base, mesh, roots):
    """Maximum fine/coarse affine height difference over every child triangle."""
    original_gradient = _face_gradients(
        base.vertices[:, :2], base.triangles, base.vertices[:, 2]
    )
    p0 = base.vertices[base.triangles[roots, 0]]
    p = mesh.vertices[mesh.triangles]
    baseline = p0[:, None, 2] + np.einsum(
        "fji,fi->fj", p[:, :, :2] - p0[:, None, :2], original_gradient[roots]
    )
    return float(np.max(abs(p[:, :, 2] - baseline), initial=0))


def refine_ground(
    mesh,
    target,
    *,
    protected_faces=0,
    method="midpoint",
    grade=2 / 3,
    error_m=0.25,
    minimum_gain_m=0.05,
    max_new_vertices=2500,
    max_faces=65536,
    max_passes=4,
    max_quality_queries=1000000,
):
    """Refine a valid fixed height plan; every old vertex stays exactly unchanged.

    Edge points split both adjacent soil faces; road/external edges stay protected.
    Quartiles probe three dyadic positions and count all target queries against
    the existing quality capacity, with at most65536 live expanded candidates.
    Centroids are the equal-cost comparison. Face-disjoint batches keep analytic
    one-free-height intervals independent. Priority is a local area/gain heuristic,
    not ROAM optimality, a DEM supremum bound or a smoothness certificate.
    """
    began = time.perf_counter()
    if not isinstance(mesh, Mesh) or method not in METHODS:
        raise ValueError("owned height mesh and known refinement method required")
    if any(type(v) is not int or v < 0 for v in (protected_faces, max_new_vertices)):
        raise ValueError(
            "nonnegative integer protected prefix and vertex budget required"
        )
    if any(
        type(v) is not int or v <= 0
        for v in (max_faces, max_passes, max_quality_queries)
    ) or protected_faces > len(mesh.triangles):
        raise ValueError(
            "positive face/pass caps and existing protected prefix required"
        )
    if not all(np.isfinite(v) and v > 0 for v in (grade, error_m, minimum_gain_m)):
        raise ValueError("positive finite grade/error/gain required")
    if len(mesh.triangles) > max_faces:
        raise ValueError("refinement input exceeds face budget")
    gradient = _face_gradients(
        mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2]
    )
    if np.any(np.linalg.norm(gradient[protected_faces:], axis=1) > grade * 1.0000001):
        raise ValueError("refinement input violates the soil-grade requirement")
    base, roots = mesh, np.arange(len(mesh.triangles))
    steps, inserted, quality_queries, reason = [], 0, 0, ""
    probe_queries = 0
    expanded = method == "quartiles"
    for _ in range(max_passes):
        capacity = min(
            max_new_vertices - inserted, max(0, (max_faces - len(mesh.triangles)) // 2)
        )
        if not capacity:
            reason = "refinement_budget_exhausted"
            break
        remaining = max_quality_queries - quality_queries - probe_queries
        try:
            candidates = _candidates(
                mesh,
                protected_faces,
                method,
                maximum_candidates=min(65536, remaining) if expanded else 65536,
            )
        except ValueError as error:
            if str(error) != "refinement_probe_budget":
                raise
            reason = "candidate_query_budget_exhausted"
            break
        if not len(candidates["xy"]):
            reason = "no_unprotected_candidates"
            break
        rank, delta, report = _rank(
            mesh, candidates, target, grade, error_m, minimum_gain_m
        )
        if expanded:
            probe_queries += len(candidates["xy"])
        capacity = min(
            capacity,
            (max_quality_queries - quality_queries - probe_queries)
            // (45 * candidates["parents"].shape[1]),
        )
        if not capacity:
            reason = "quality_query_budget_exhausted"
            break
        selected, busy = [], np.zeros(len(mesh.triangles), dtype=bool)
        for i in rank:
            parents = candidates["parents"][i]
            if not np.any(busy[parents]):
                selected.append(int(i))
                busy[parents] = True
                if len(selected) == capacity:
                    break
        if not selected:
            reason = "no_measured_gain_within_grade"
            break
        adjusted, queried = _quality_delta(
            mesh, candidates, selected, delta, target, method
        )
        quality_queries += queried
        report["quality_reduced_candidates"] = int(
            np.count_nonzero(abs(adjusted) < abs(delta[selected]) - 1e-10)
        )
        delta[selected] = adjusted
        selected = [i for i in selected if abs(delta[i]) >= minimum_gain_m]
        if not selected:
            reason = "no_gain_after_local_quality_check"
            break
        mesh, roots = _split(mesh, roots, candidates, selected, delta, method)
        inserted += len(selected)
        report.update(
            inserted=len(selected),
            maximum_step_change_m=float(np.max(abs(delta[selected]))),
        )
        steps.append(report)
    if not reason:
        reason = "pass_budget_exhausted"
    bound = vertical_lod_difference(base, mesh, roots)
    report = {
        "method": method,
        "reason": reason,
        "steps": steps,
        "new_vertices": inserted,
        "faces": len(mesh.triangles),
        "quality_queries": quality_queries,
        "maximum_vertical_LOD_difference_m": bound,
        "refinement_ms": (time.perf_counter() - began) * 1000,
    }
    if expanded:
        report.update(
            candidate_target_queries=probe_queries,
            total_target_queries=probe_queries + quality_queries,
            maximum_live_probe_candidates=65536,
        )
    return mesh, roots, report
