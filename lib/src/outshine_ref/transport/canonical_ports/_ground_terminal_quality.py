"""Recheck the complete finite product after source-bound terminal construction."""

from dataclasses import fields, replace

import numpy as np
import shapely

from ..port_heights._quality import quality
from ..port_heights._world import finite_ports, normal_gradients


def qualify_terminal_surface(field, mesh, report, prior_limit):
    copied_payload = 0
    inputs = report["source_input"]
    xyz = mesh.vertices[mesh.triangles]
    a, b = xyz[:, 1, :2] - xyz[:, 0, :2], xyz[:, 2, :2] - xyz[:, 0, :2]
    if not np.isfinite(xyz).all() or np.any(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0] <= 0):
        raise ValueError("terminal_surface_orientation")
    if not shapely.coverage_is_valid(shapely.polygons(xyz[:, :, :2])):
        raise ValueError("terminal_surface_coverage")
    if hasattr(field, "plan"):
        plan = field.plan
        roots = {
            tuple(sorted(map(int, edge))): int(i)
            for edge, i in zip(plan.port_edges, plan.port_ids, strict=True)
        }
        _, checked, errors = quality(
            plan.field,
            plan.plane_coefficients,
            {
                "xy": mesh.vertices[:, :2],
                "faces": mesh.triangles,
                "parts": plan.face_parts,
                "frames": plan.grade_frames,
                "heights": mesh.vertices[:, 2],
                "port_roots": roots,
            },
            inputs.part_roles,
        )
        if not checked["geometry_complete"] or not checked["whole_world_valid"]:
            raise ValueError("terminal_surface_quality")
        vertex_map, parts = plan.original_vertex_map, plan.face_parts
        copied_payload = 2 * (mesh.vertices.nbytes + mesh.triangles.nbytes) + sum(
            getattr(plan, member.name).nbytes
            for member in fields(plan)
            if isinstance(getattr(plan, member.name), np.ndarray)
        )
        field.plan = replace(plan, mesh=mesh, height_error_bounds_m=errors)
        mesh = field.plan.mesh
        parts = field.plan.face_parts
    else:
        gradients = normal_gradients(
            mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2]
        )
        normalized = np.linalg.norm(
            np.einsum("fij,fj->fi", inputs.grade_frames, gradients), axis=1
        )
        roots = {
            tuple(sorted(map(int, edge))): i for i, edge in enumerate(inputs.port_edges)
        }
        bad, contacts = finite_ports(
            mesh.vertices[:, :2],
            mesh.triangles,
            inputs.face_parts,
            mesh.vertices[:, 2],
            roots,
            inputs.part_roles,
        )
        if (
            not np.isfinite(normalized).all()
            or float(max(normalized)) > 1.0000001
            or len(bad)
        ):
            raise ValueError("terminal_surface_quality")
        checked = {
            "geometry_complete": True,
            "maximum_height_chord_bound_m": 0.0,
            "maximum_normalized_continuous_grade": field.report[
                "maximum_normalized_continuous_grade"
            ],
            "maximum_normalized_finite_grade": float(max(normalized)),
            "finite_contacts": contacts,
        }
        vertex_map, parts = np.arange(len(mesh.vertices)), inputs.face_parts
    prior_error = float(
        np.max(abs(mesh.vertices[vertex_map, 2] - inputs.surface.vertices[:, 2]))
    )
    if prior_error > prior_limit:
        raise ValueError("terminal_original_prior_error")
    checked["maximum_original_prior_vertex_error_m"] = prior_error
    checked["scope"] = (
        "Whole finite field and corresponding unchanged original DEM priors; no dense-DEM/terrain/Bridge/World admission"
    )
    return mesh, parts, checked, copied_payload
