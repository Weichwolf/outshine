"""Independent Float64 3D ray solves on saved actual-host curved triangle surfaces."""

import argparse
import json
from pathlib import Path

import numpy as np
from cloud_experiment import view_bounds
from connected_reader import read_host
from foliage_selection import directions
from shoot_plan import normal_frame
from surface_fragments import mesh_hits


def ray_hits(mesh, camera, bounds, size, pixels):
    """Solve triangle edge coordinates and ray travel in world3D, no 2D raster kernel."""
    lower, upper = bounds
    xy = lower + np.column_stack((pixels % size + 0.5, pixels // size + 0.5)) / size * (
        upper - lower
    )
    eye_depth = float(np.max(mesh["vertices"] @ camera[:, 2])) + 1
    origins = np.column_stack((xy, np.full(len(xy), eye_depth))) @ camera.T
    result = {}
    boundary = set()
    max_condition = 0.0
    for index, triangle in enumerate(mesh["vertices"][mesh["faces"]]):
        a, b, c = triangle
        matrix = np.column_stack((b - a, c - a, camera[:, 2]))
        condition = float(np.linalg.cond(matrix))
        if not np.isfinite(condition) or condition > 1e12:
            raise ValueError("independent ray fixture is numerically unresolved")
        max_condition = max(max_condition, condition)
        u, v, travel = np.linalg.solve(matrix, (origins - a).T)
        barycentric = np.column_stack((1 - u - v, u, v))
        minimum = barycentric.min(axis=1)
        boundary.update(pixels[(abs(minimum) < 1e-8) & (travel >= 0)].tolist())
        for j in np.flatnonzero((minimum >= 0) & (travel >= 0)):
            result[(index, int(pixels[j]))] = eye_depth - float(travel[j])
    return result, boundary, max_condition


def check_mesh(mesh, camera, bounds, size, pixels, *, owner=0):
    expected, boundary, condition = ray_hits(mesh, camera, bounds, size, pixels)
    # Interior-ray comparison is explicit; shared edges have separate analytic tests.
    allowed = set(pixels.tolist()) - boundary
    expected = {k: v for k, v in expected.items() if k[1] in allowed}
    tri, _, pix, depth, _ = mesh_hits(mesh, camera, bounds, size, owner=owner)
    actual = {
        (int(t), int(p)): float(d)
        for t, p, d in zip(tri, pix, depth, strict=True)
        if int(p) in allowed
    }
    if actual.keys() != expected.keys():
        raise AssertionError(
            f"ray/raster hit sets differ: missing{len(expected.keys() - actual.keys())} "
            f"extra{len(actual.keys() - expected.keys())}"
        )
    error = max((abs(v - actual[k]) for k, v in expected.items()), default=0)
    if error > 1e-10:
        raise AssertionError("ray/raster depth differs by more than1e-10m")
    return {
        "tested_pixels": len(allowed),
        "explicit_boundary_pixels": len(boundary),
        "matching_hit_pairs": len(expected),
        "maximum_depth_error_m": error,
        "maximum_solve_condition": condition,
    }


def study(inputs):
    results = []
    pixels = np.random.default_rng(704).choice(64 * 64, 512, replace=False)
    for node in (17, 2178, 3925, 5471):
        source, surfaces = read_host(inputs, node)
        meshes = [
            ("source", source, source["ids"][source["organ_faces"]]),
            *((str(i), s["mesh"], s["owner"]) for i, s in enumerate(surfaces)),
        ]
        points = np.vstack([m["vertices"] for _, m, _ in meshes])
        for query, direction in enumerate(directions(4)):
            basis = normal_frame(direction, (0, 0, 1))
            camera = np.column_stack((basis[:, 0], basis[:, 2], basis[:, 1]))
            bounds = view_bounds(points, direction)
            for name, mesh, owner in meshes:
                results.append(
                    {
                        "host": node,
                        "query": query,
                        "surface": name,
                        **check_mesh(mesh, camera, bounds, 64, pixels, owner=owner),
                    }
                )
    return {
        "cases": results,
        "scope": "Independent3D Float64 linear solves, actual saved83leaves and4source hosts,4 spherical views,512 reproducible64x64 pixel centres each. Exact interior hit-set equality; boundary pixels explicitly reported, not admitted by this oracle. Analytic shared-edge controls are separate. Not an exact-predicate proof or device rasterization admission.",
    }


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--inputs", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    if not a.output.resolve().is_relative_to(Path("/tmp")):
        p.error("private outputs must be under/tmp")
    a.output.write_text(json.dumps(study(a.inputs), indent=2) + "\n")
