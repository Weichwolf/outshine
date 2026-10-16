"""Fixed independent first-exit ray probes; no star-shape or global error claim."""

import numpy as np
from curved_fork import CurveBranch, CurvedFork
from fork_checks import ray_mesh_radii
from spine_frames import unit


def field_from_plan(plan):
    return CurvedFork(
        tuple(
            CurveBranch(
                b["points_m"], b["radii_m"], b["centre_plus_radius_chord_bound_m"]
            )
            for b in plan["branches"]
        ),
        plan["blend_m2"],
    )


def probe_directions(field):
    origin = np.asarray(field.origin)
    directions = [unit(np.random.default_rng(443).normal(size=(256, 3)))]
    for branch in field.branches:
        for endpoint, radius in (
            (branch.points[0], branch.radii[0]),
            (branch.points[-1], branch.radii[-1]),
        ):
            target = np.asarray(endpoint) - origin
            if np.linalg.norm(target) < 1e-10:
                continue
            forward = unit(target)
            side = unit(np.cross(forward, np.eye(3)[np.argmin(abs(forward))]))
            up = np.cross(forward, side)
            angle = np.arange(16) * 2 * np.pi / 16
            directions.append(
                unit(
                    target
                    + 0.95
                    * radius
                    * (np.cos(angle)[:, None] * side + np.sin(angle)[:, None] * up)
                )
            )
    return np.vstack(directions)


def first_exits(field, directions, steps):
    origin = np.asarray(field.origin)
    assert field.evaluate(origin[None])[0] > 0
    grid = np.linspace(0, field.ray_extent * 1.01, steps + 1)
    low, high = [], []
    for start in range(0, len(directions), 32):
        rays = directions[start : start + 32]
        values = field.evaluate(
            (origin + rays[:, None] * grid[None, :, None]).reshape(-1, 3)
        ).reshape(len(rays), -1)
        assert np.all(np.any(values[:, 1:] < 0, axis=1))
        first = np.argmax(values[:, 1:] < 0, axis=1) + 1
        low.extend(grid[first - 1])
        high.extend(grid[first])
    low, high = np.array(low), np.array(high)
    for _ in range(42):
        mid = (low + high) / 2
        inside = field.evaluate(origin + directions * mid[:, None]) >= 0
        low = np.where(inside, mid, low)
        high = np.where(inside, high, mid)
    return (low + high) / 2


def ray_accuracy(mesh, plan):
    field = field_from_plan(plan)
    rays = probe_directions(field)
    reference = first_exits(field, rays, 256)
    refined = first_exits(field, rays, 512)
    bracket_change = float(np.max(abs(refined - reference)))
    if bracket_change > 1e-6:
        raise ValueError("fixed first-exit brackets do not converge at1micrometre")
    hit = ray_mesh_radii(mesh, rays, field.origin)
    error = abs(hit - refined)
    return {
        "rays": len(rays),
        "scan_refinement_change_m": bracket_change,
        "maximum_ray_probe_error_m": float(error.max()),
        "rms_ray_probe_error_m": float(np.sqrt(np.mean(error**2))),
        "two_mm_probe_contract_met": bool(error.max() <= 0.002),
    }
