"""Check direct crown tessellation, continuous error bounds and nested positions."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from crown_surface import Crown
from cube_surface import face_axes
from demand import Camera


def coarse_interpolate(crown, parameters, level, bands=None):
    divisions = 1 << level
    result = np.empty_like(parameters)
    axes = np.argmax(np.abs(parameters), axis=1)
    for axis in range(3):
        for sign in (-1, 1):
            mask = (axes == axis) & (parameters[:, axis] * sign > 0)
            points = parameters[mask]
            normal, first, second = face_axes(axis, sign)
            coordinates = np.column_stack((points @ first, points @ second))
            grid = np.clip((coordinates + 1) * divisions / 2, 0, divisions)
            indices = np.minimum(np.floor(grid), divisions - 1)
            u, v = (grid - indices).T
            origin = indices * 2 / divisions - 1
            corners = np.array([[0, 0], [1, 0], [1, 1], [0, 1]])
            quads = origin[:, None, :] + corners * (2 / divisions)
            cube = normal + quads[..., :1] * first + quads[..., 1:] * second
            positions, _ = crown.evaluate(cube, bands)
            a, b, c, d = positions.transpose(1, 0, 2)
            below = (1 - u[:, None]) * a + (u - v)[:, None] * b + v[:, None] * c
            above = (1 - v[:, None]) * a + u[:, None] * c + (v - u)[:, None] * d
            result[mask] = np.where((v <= u)[:, None], below, above)
    return result


def valid_mesh(crown, mesh):
    vertices, faces = mesh["vertices"], mesh["faces"]
    triangles = vertices[faces]
    cross = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    assert np.all(np.linalg.norm(cross, axis=1) > 0)
    assert np.all(np.sum(cross * (triangles.mean(axis=1) - crown.centre), axis=1) > 0)
    assert np.all(np.abs((vertices - crown.centre) / crown.radii) <= 1 + 1e-12)
    edges = np.concatenate((faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]))
    _, inverse, counts = np.unique(
        np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    assert np.all(counts == 2)
    signs = np.where(edges[:, 0] < edges[:, 1], 1, -1)
    assert np.all(np.bincount(inverse, weights=signs) == 0)
    np.testing.assert_allclose(np.linalg.norm(mesh["normals"], axis=1), 1, atol=1e-12)


def normals_check(crown):
    normal, first, second = face_axes(2, 1)
    uv = np.random.default_rng(91).uniform(-0.9, 0.9, (128, 2))
    parameters = normal + uv[:, :1] * first + uv[:, 1:] * second
    _, normals = crown.evaluate(parameters)
    for tangent in (first, second):
        delta = tangent * 1e-5
        above, _ = crown.evaluate(parameters + delta)
        below, _ = crown.evaluate(parameters - delta)
        derivative = (above - below) / (2e-5)
        cosine = np.sum(derivative * normals, axis=1) / np.linalg.norm(
            derivative, axis=1
        )
        assert np.max(np.abs(cosine)) < 1e-8


def run_case(name, profile, seed, output):
    height, spread, bole = (
        profile["height_m"],
        profile["spread_m"],
        profile["bole_frac"],
    )
    crown = Crown(
        (spread / 2, spread / 2, height * (1 - bole) / 2),
        (0, 0, height * (1 + bole) / 2),
        seed,
    )
    normals_check(crown)
    reference = crown.mesh(6)
    reference_keys = {
        tuple(p): index for index, p in enumerate(reference["parameters"])
    }
    cases = []
    for level in range(6):
        durations = []
        for _ in range(3):
            began = time.perf_counter()
            mesh = crown.mesh(level)
            durations.append((time.perf_counter() - began) * 1000)
        valid_mesh(crown, mesh)
        reused = [reference_keys[tuple(p)] for p in mesh["parameters"]]
        np.testing.assert_array_equal(mesh["vertices"], reference["vertices"][reused])
        approximate = coarse_interpolate(crown, reference["parameters"], level)
        measured = float(
            np.max(np.linalg.norm(approximate - reference["vertices"], axis=1))
        )
        bound = crown.interpolation_bound(level)
        assert measured <= bound
        if seed == 2:
            np.savez_compressed(output / f"{name}-level{level}.npz", **mesh)
        cases.append(
            {
                "level": level,
                "triangles": len(mesh["faces"]),
                "arrays_bytes": sum(v.nbytes for v in mesh.values()),
                "generation_ms_samples": durations,
                "generation_ms_median": float(np.median(durations)),
                "max_reference_error_m": measured,
                "continuous_error_bound_m": bound,
            }
        )
    choices = []
    for distance in (50, 180, 800, 15000):
        camera = Camera((0, -distance, height * 0.6), (0, 0, height * 0.6))
        level, error, satisfied = crown.choose_level(camera)
        assert satisfied
        selected, bands, reduced_error, satisfied = crown.choose_product(camera)
        assert satisfied and selected <= level
        approximate = coarse_interpolate(
            crown, reference["parameters"], selected, bands
        )
        measured = float(
            np.max(np.linalg.norm(approximate - reference["vertices"], axis=1))
        )
        assert measured <= crown.interpolation_bound(selected, bands)
        fine_local = (reference["vertices"] - camera.position) @ camera.axes.T
        coarse_local = (approximate - camera.position) @ camera.axes.T
        focal = camera.height / (2 * np.tan(np.deg2rad(camera.fov_y) / 2))
        fine_screen = focal * fine_local[:, :2] / fine_local[:, 2:]
        coarse_screen = focal * coarse_local[:, :2] / coarse_local[:, 2:]
        screen_error = float(
            np.max(np.linalg.norm(fine_screen - coarse_screen, axis=1))
        )
        assert screen_error <= reduced_error
        choices.append(
            {
                "distance_m": distance,
                "full_band_level": level,
                "full_band_triangles": 12 * (1 << (2 * level)),
                "full_band_bound_px": float(error),
                "selected_level": selected,
                "selected_bands": bands,
                "selected_triangles": 12 * (1 << (2 * selected)),
                "selected_bound_px": float(reduced_error),
                "max_reference_error_px": screen_error,
                "pixel_tolerance": 1.0,
            }
        )
    _, error, satisfied = crown.choose_level(
        Camera((0, -50, 10), (0, 0, 10)), maximum=0
    )
    assert not satisfied and error > 1
    return {
        "species": name,
        "seed": seed,
        "nested_positions_exact": True,
        "closed_outward_mesh": True,
        "analytic_normals_checked": True,
        "cases": cases,
        "camera_choices": choices,
        "capped_quality_remains_unsatisfied": True,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    sources = [
        Path("src/assets/world/species") / f"{name}.json"
        for name in ("oak", "beech", "birch")
    ]
    cases = [
        run_case(path.stem, json.loads(path.read_text()), seed, args.output)
        for path in sources
        for seed in (2, 7, 19)
    ]
    report = {
        "cases": cases,
        "bound_scope": "continuous radial envelope geometry; no foliage coverage, shading or shadow error bound",
        "fine_growth_required_for_coarse_product": False,
        "numpy_version": np.__version__,
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("*.py")
        },
        "profile_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sources
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True).strip()
        ),
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
    }
    (args.output / "crown.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{len(cases)} crown cases PASS", flush=True)


if __name__ == "__main__":
    main()
