"""Finite local Float32 height repair after the unchanged joint cone fit."""

import itertools
import time

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightFit
from outshine_ref.transport.heights import _gradient_weights
from stair_joint_height_fit import fit_joint_levels, level_groups

MAX_CHANGE_M = 0.005
MAX_CANDIDATES = 100


def grades(coefficients, faces, heights):
    values = heights[..., faces]
    difference = values - values[..., :1]
    return np.linalg.norm(
        np.einsum("fij,...fj->...fi", coefficients, difference), axis=-1
    )


def proposals(faces, coefficients, heights, raw, alias, bad, remaining):
    variables = np.unique(alias[faces[bad]])
    members = np.flatnonzero(np.isin(alias, variables))
    affected = np.flatnonzero(np.isin(faces, members).any(axis=1))
    used, local_faces = np.unique(faces[affected], return_inverse=True)
    choices = []
    for variable in variables:
        value = np.float32(heights[variable])
        choices.append(
            [
                float(value),
                float(np.nextafter(value, np.float32(-np.inf))),
                float(np.nextafter(value, np.float32(np.inf))),
            ]
        )
    values = np.array(list(itertools.islice(itertools.product(*choices), remaining)))
    candidates = np.broadcast_to(heights[used], (len(values), len(used))).copy()
    for column, variable in enumerate(variables):
        candidates[:, alias[used] == variable] = values[:, column, None]
    actual = grades(coefficients[affected], local_faces.reshape(-1, 3), candidates)
    errors = abs(candidates - raw[used])
    valid = np.isfinite(candidates).all(axis=1) & (errors.max(axis=1) <= MAX_CHANGE_M)
    return (
        used,
        affected,
        candidates,
        actual,
        errors,
        valid,
        {
            "candidate_count": len(values),
            "candidate_face_tests": len(values) * len(affected),
            "affected_faces": len(affected),
            "candidate_height_bytes": candidates.nbytes,
            "candidate_grade_bytes": actual.nbytes,
            "candidate_error_bytes": errors.nbytes,
        },
    )


def repair(surface, frames, groups, levels, *, seconds, max_candidates=MAX_CANDIDATES):
    began = time.perf_counter()
    report = {
        "valid": False,
        "maximum_change_budget_m": MAX_CHANGE_M,
        "candidate_budget": max_candidates,
        "candidates": 0,
        "candidate_face_tests": 0,
        "batches": [],
    }
    if type(max_candidates) is not int or not 0 <= max_candidates <= MAX_CANDIDATES:
        raise ValueError("bounded finite candidate count required")
    if not np.isfinite(seconds) or seconds < 0:
        raise ValueError("finite nonnegative repair time required")
    xyz, faces = surface.vertices, surface.triangles
    if not np.array_equal(xyz[:, :2], xyz[:, :2].astype("f4").astype(float)):
        return None, {**report, "reason": "source_XY_not_exact_Float32"}
    if np.max(abs(xyz[:, 2]), initial=0) > np.finfo("f4").max:
        return None, {**report, "reason": "height_outside_Float32"}
    heights = xyz[:, 2].astype("f4").astype(float)
    alias = np.arange(len(xyz))
    for group, level in zip(groups, levels, strict=True):
        heights[group], alias[group] = level, group[0]
    if np.max(abs(heights - xyz[:, 2]), initial=0) > MAX_CHANGE_M:
        return None, {**report, "reason": "initial_rounding_exceeds_change_budget"}
    coefficients = np.einsum(
        "fij,fjk->fik", frames, _gradient_weights(xyz[:, :2], faces)
    )
    current = grades(coefficients, faces, heights)
    if not np.isfinite(current).all():
        return None, {**report, "reason": "nonfinite_actual_height_grades"}
    while np.any(current > 1):
        if (
            report["candidates"] >= max_candidates
            or time.perf_counter() - began >= seconds
        ):
            return None, {**report, "reason": "local_rounding_budget_exhausted"}
        used, affected, candidates, actual, errors, valid, note = proposals(
            faces,
            coefficients,
            heights,
            xyz[:, 2],
            alias,
            int(np.flatnonzero(current > 1)[0]),
            max_candidates - report["candidates"],
        )
        report["candidates"] += note["candidate_count"]
        report["candidate_face_tests"] += note["candidate_face_tests"]
        unchanged = np.ones(len(faces), bool)
        unchanged[affected] = False
        outside_bad = int(np.count_nonzero(current[unchanged] > 1))
        outside_max = float(current[unchanged].max(initial=0))
        old_score = (
            int(np.count_nonzero(current > 1)),
            max(0.0, float(current.max()) - 1),
        )
        maxima = np.maximum(actual.max(axis=1), outside_max)
        scores = [
            (
                int(np.count_nonzero(actual[i] > 1)) + outside_bad,
                max(0.0, float(maxima[i]) - 1),
                float(errors[i].max(initial=0)),
                float(errors[i].sum()),
                i,
            )
            for i in np.flatnonzero(valid)
        ]
        if not scores or min(scores)[:2] >= old_score:
            return None, {**report, "reason": "no_improving_adjacent_Float32_candidate"}
        best = min(scores)[-1]
        heights[used], current[affected] = candidates[best], actual[best]
        report["batches"].append(
            {**note, "remaining_bad_faces": int(np.count_nonzero(current > 1))}
        )
    vertices = xyz.copy()
    vertices[:, 2] = heights
    if (
        not np.isfinite(vertices).all()
        or not np.array_equal(vertices, vertices.astype("f4").astype(float))
        or any(np.ptp(heights[g]) != 0 for g in groups)
        or np.max(abs(heights - xyz[:, 2]), initial=0) > MAX_CHANGE_M
    ):
        return None, {**report, "reason": "invalid_repaired_height_candidate"}
    return Mesh(vertices, faces), {
        **report,
        "valid": True,
        "reason": "",
        "maximum_normalized_grade": float(current.max()),
        "maximum_change_m": float(abs(heights - xyz[:, 2]).max(initial=0)),
        "changed_rounded_vertices": int(
            np.count_nonzero(heights != xyz[:, 2].astype("f4"))
        ),
        "coefficients_bytes": coefficients.nbytes,
        "alias_bytes": alias.nbytes,
        "scope": "At most 100 adjacent-height proposals; flat groups share one variable. Only incident faces evaluated per proposal; final whole mesh remains subject to the normal independent audit. Time checked between bounded batches; no optimality or general success guarantee.",
    }


def fit_rounded_levels(surface, frames, plans, budget):
    began = time.perf_counter()
    result, levels = fit_joint_levels(surface, frames, plans, budget)
    if result.mesh is None:
        return result, levels
    remaining = max(0.0, budget.time_limit_s - (time.perf_counter() - began))
    groups = level_groups(surface.vertices[:, :2], plans)
    mesh, proof = repair(result.mesh, frames, groups, levels, seconds=remaining)
    return HeightFit(
        mesh,
        {
            "method": "Joint cone with finite Float32 repair",
            "continuous": dict(result.report),
            "rounding": proof,
            "valid": mesh is not None,
            "reason": proof["reason"],
            "total_ms": (time.perf_counter() - began) * 1000,
        },
    ), (None if mesh is None else np.array([mesh.vertices[g[0], 2] for g in groups]))
