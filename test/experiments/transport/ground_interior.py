"""Bounded coordinate sweeps over independent refinement insertion batches."""

import numpy as np
from ground_refinement import _intervals, vertical_lod_difference
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import _face_gradients, _gradient_weights


def _blocks(mesh, fixed_vertices, counts, protected_faces):
    colors = np.r_[
        np.full(fixed_vertices, -1), np.repeat(np.arange(len(counts)), counts)
    ]
    face_colors = colors[mesh.triangles]
    if np.any(face_colors[:protected_faces] >= 0):
        raise ValueError("protected faces contain movable vertices")
    for a, b in ((0, 1), (1, 2), (2, 0)):
        if np.any((face_colors[:, a] >= 0) & (face_colors[:, a] == face_colors[:, b])):
            raise ValueError("insertion batch contains adjacent vertices")
    weights = _gradient_weights(mesh.vertices[:, :2], mesh.triangles)
    blocks = []
    for color in range(len(counts)):
        nodes = np.flatnonzero(colors == color)
        positions = np.flatnonzero(face_colors.ravel() == color)
        faces = positions // 3
        local = np.searchsorted(nodes, mesh.triangles.ravel()[positions])
        if len(np.unique(local)) != len(nodes):
            raise ValueError("movable vertex has no incident face")
        blocks.append((nodes, faces, local, weights[faces, :, positions % 3]))
    return blocks


def _sweep(vertices, gradient, blocks, targets, fixed_vertices, grade):
    maximum = 0.0
    for nodes, faces, local, coefficient in blocks:
        lo, hi, possible = _intervals(
            gradient[faces, None, :], coefficient[:, None, :], grade
        )
        lower, upper = np.full(len(nodes), -np.inf), np.full(len(nodes), np.inf)
        accepted = np.ones(len(nodes), dtype=bool)
        np.maximum.at(lower, local, lo)
        np.minimum.at(upper, local, hi)
        np.logical_and.at(accepted, local, possible)
        desired = targets[nodes - fixed_vertices] - vertices[nodes, 2]
        delta = np.where(
            accepted & np.isfinite(desired), np.clip(desired, lower, upper), 0.0
        )
        vertices[nodes, 2] += delta
        # Each face occurs at most once in a proper insertion batch.
        gradient[faces] += coefficient * delta[local, None]
        maximum = max(maximum, float(abs(delta).max(initial=0)))
    return maximum


def relax_interior(
    mesh,
    target,
    *,
    fixed_vertices,
    insertion_counts,
    protected_faces=0,
    grade=2 / 3,
    maximum_sweeps=2,
    maximum_incidence_updates=32768,
):
    """Keep a valid base and all fixed XYZ; improve finite soft height targets.

    One analytic height interval per incident face bounds its complete gradient.
    Independent insertion batches reuse the refinement coloring. Each coordinate
    moves toward its observation, so every positive diagonal quadratic objective
    is nonincreasing. This is not a coupled optimum or a DEM error certificate:
    an active gradient can prevent all separate moves despite a joint improvement.
    Work caps count face/vertex incidences; they do not certify wall time.
    """
    counts = tuple(insertion_counts)
    capacities = (
        fixed_vertices,
        protected_faces,
        maximum_sweeps,
        maximum_incidence_updates,
    )
    if any(type(v) is not int or v < 0 for v in capacities):
        raise ValueError("nonnegative integer work limits and fixed prefixes required")
    if (
        not isinstance(mesh, Mesh)
        or fixed_vertices > len(mesh.vertices)
        or protected_faces > len(mesh.triangles)
        or any(type(v) is not int or v <= 0 for v in counts)
        or sum(counts) != len(mesh.vertices) - fixed_vertices
        or not np.isfinite(grade)
        or grade <= 0
    ):
        raise ValueError("valid mesh, insertion partition and positive grade required")
    gradient = _face_gradients(
        mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2]
    )
    if not np.isfinite(gradient).all() or np.any(
        np.linalg.norm(gradient[protected_faces:], axis=1) > grade * 1.0000001
    ):
        raise ValueError("input violates the whole soil grade")
    report = {
        "method": "independent_insertion_coordinate_sweeps",
        "groups": len(counts),
        "sweeps": 0,
        "incidence_updates": 0,
        "target_queries": 0,
        "missing_vertex_ids": [],
        "maximum_step_changes_m": [],
        "accepted": True,
        "reason": "work_budget_exhausted",
    }
    if not counts or not maximum_sweeps or not maximum_incidence_updates:
        return mesh, report
    blocks = _blocks(mesh, fixed_vertices, counts, protected_faces)
    work = sum(len(block[1]) for block in blocks)
    sweeps = min(maximum_sweeps, maximum_incidence_updates // work)
    if not sweeps:
        return mesh, report
    targets = np.asarray(target(mesh.vertices[fixed_vertices:, :2]), dtype=float)
    if targets.shape != (len(mesh.vertices) - fixed_vertices,):
        raise ValueError("one height observation per movable vertex required")
    report["target_queries"] = len(targets)
    report["missing_vertex_ids"] = (
        np.flatnonzero(~np.isfinite(targets)) + fixed_vertices
    ).tolist()
    vertices = mesh.vertices.copy()
    for _ in range(sweeps):
        change = _sweep(vertices, gradient, blocks, targets, fixed_vertices, grade)
        report["sweeps"] += 1
        report["incidence_updates"] += work
        report["maximum_step_changes_m"].append(change)
        if change == 0:
            report["reason"] = "no_coordinate_move"
            break
    actual = _face_gradients(vertices[:, :2], mesh.triangles, vertices[:, 2])
    if not np.isfinite(vertices).all() or np.any(
        np.linalg.norm(actual[protected_faces:], axis=1) > grade * 1.0000001
    ):
        report.update(accepted=False, reason="whole_gradient_postcheck_failed")
        return mesh, report
    report["maximum_soil_grade"] = float(
        np.linalg.norm(actual[protected_faces:], axis=1).max(initial=0)
    )
    return Mesh(vertices, mesh.triangles), report


def relax_contact(road, result, target, **limits):
    """Additional explicit contact path; update its whole geometric LOD bound."""
    if result.get("status") != "candidate":
        raise ValueError("qualified contact candidate required")
    mesh, report = relax_interior(
        result["mesh"],
        target,
        fixed_vertices=len(result["coarse_mesh"].vertices),
        insertion_counts=[step["inserted"] for step in result["refinement"]["steps"]],
        protected_faces=len(road.triangles),
        grade=result["limits"].soil_grade,
        **limits,
    )
    bound = vertical_lod_difference(
        result["coarse_mesh"], mesh, result["refinement_roots"]
    )
    return result | {
        "mesh": mesh,
        "refinement": result["refinement"]
        | {"maximum_vertical_LOD_difference_m": bound},
        "interior_fit": report,
    }
