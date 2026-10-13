"""Fresh-process whole-plant cache load and independent affine view-selection checks."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from blade_surface import Blade
from forest_views import choices
from PIL import Image
from plant_plan import PlantPlan
from scipy.spatial.transform import Rotation


def root_checks(mesh, manifest):
    vertices = mesh["vertices"].reshape(
        manifest["views"], manifest["vertices_per_view"], 3
    )
    faces = mesh["faces"].reshape(manifest["views"], manifest["draw_triangles"], 3)
    worst = 0.0
    for index, direction in enumerate(mesh["directions"]):
        points = vertices[index]
        local_faces = faces[index] - index * manifest["vertices_per_view"]
        triangles = points[local_faces]
        area = np.linalg.norm(
            np.cross(
                triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
            ),
            axis=1,
        )
        assert np.all(np.isfinite(triangles)) and np.all(area > 0)
        first = np.eye(3)[np.argmin(abs(direction))]
        horizontal = first - (first @ direction) * direction
        horizontal /= np.linalg.norm(horizontal)
        vertical = np.cross(direction, horizontal)
        xy = triangles @ np.column_stack((horizontal, vertical))
        matrices = np.concatenate(
            (xy.transpose(0, 2, 1), np.ones((len(xy), 1, 3))), axis=1
        )
        weights = np.linalg.solve(
            matrices, np.broadcast_to((0, 0, 1), (len(xy), 3))[..., None]
        )[..., 0]
        inside = np.all(weights >= -1e-10, axis=1)
        assert np.any(inside)
        roots = np.einsum("ni,nij->nj", weights[inside], triangles[inside])
        error = float(np.linalg.norm(roots, axis=1).max())
        assert error < 1e-9
        worst = max(worst, error)
    return worst


def reference_indices(mesh, sites, pivot, query, perspective):
    expected = []
    for position, scale, angle in zip(
        sites["positions"], sites["scales"], sites["angles"]
    ):
        c, s = np.cos(angle), np.sin(angle)
        model = np.eye(4)
        model[:3, :3] = np.array(((c, -s, 0), (s, c, 0), (0, 0, 1))) @ np.diag(scale)
        model[:3, 3] = position
        local = (np.linalg.inv(model) @ np.r_[query, int(perspective)])[:3]
        if perspective:
            local -= pivot
        expected.append(int(np.argmax(mesh["directions"] @ local)))
    return expected


def measure(atlas, scene):
    began = time.perf_counter()
    manifest = json.loads((atlas / "plant-product.json").read_text())
    recipe = json.loads((atlas / manifest["recipe"]).read_text())
    assert (
        hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
        == manifest["plan_identity"]
    )
    with np.load(atlas / manifest["mesh"], allow_pickle=False) as stored:
        mesh = {key: stored[key] for key in stored.files}
    maps = []
    for key, filename in manifest["textures"].items():
        with Image.open(atlas / filename) as image:
            maps.append(
                np.asarray(image.convert("L" if key == "leaf_weight" else "RGBA"))
            )
    load_ms = (time.perf_counter() - began) * 1000
    for value in (*mesh.values(), *maps):
        value.setflags(write=False)
    saved = json.loads(scene.read_text())
    with np.load(scene.parent / saved["sites"], allow_pickle=False) as stored:
        sites = {key: stored[key] for key in stored.files}
    lower, upper = np.asarray(manifest["bounds_m"])
    pivot = (lower + upper) / 2
    query = np.asarray(saved["camera"])
    actual = choices(
        mesh, sites["positions"], sites["scales"], sites["angles"], pivot, eye=query
    )
    expected = reference_indices(mesh, sites, pivot, query, True)
    np.testing.assert_array_equal(actual, expected)
    np.testing.assert_array_equal(actual, sites["camera_views"])
    sunlight = Rotation.from_euler("xyz", saved["sun_rotation"]).apply((0, 0, 1))
    sun = choices(
        mesh,
        sites["positions"],
        sites["scales"],
        sites["angles"],
        pivot,
        direction=sunlight,
    )
    np.testing.assert_array_equal(
        sun, reference_indices(mesh, sites, pivot, sunlight, False)
    )
    np.testing.assert_array_equal(sun, sites["sun_views"])
    root_error = root_checks(mesh, manifest)
    reverse = choices(
        mesh,
        sites["positions"][::-1],
        sites["scales"][::-1],
        sites["angles"][::-1],
        pivot,
        eye=query,
    )
    np.testing.assert_array_equal(actual, reverse[::-1])
    times = []
    for angle in np.arange(30) * 2 * np.pi / 30:
        eye = (
            130 * np.cos(angle) + 245 * np.sin(angle),
            130 * np.sin(angle) - 245 * np.cos(angle),
            85,
        )
        began = time.perf_counter()
        selected = choices(
            mesh, sites["positions"], sites["scales"], sites["angles"], pivot, eye=eye
        )
        times.append((time.perf_counter() - began) * 1000)
        assert len(selected) == len(sites["positions"]) and np.all(
            (0 <= selected) & (selected < manifest["views"])
        )
    return {
        "cache_load_ms": load_ms,
        "decoded_texture_bytes": sum(value.nbytes for value in maps),
        "mesh_array_bytes": sum(value.nbytes for value in mesh.values()),
        "sites": len(actual),
        "samples_ms": times,
        "p50_ms": float(np.percentile(times, 50)),
        "p95_ms": float(np.percentile(times, 95)),
        "p99_ms": float(np.percentile(times, 99)),
        "affine_inverse_reference_matches": True,
        "sun_affine_inverse_reference_matches": True,
        "maximum_root_error_m": root_error,
        "reversed_requests_match": True,
        "scope": "fresh-process CPU cache decode; 30 CPU camera selections, no geometry generation, GPU upload or frame-rate claim",
        "plan_identity": manifest["plan_identity"],
        "peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--atlas", type=Path, default=Path("build/experiments/vegetation/plant-atlas")
    )
    parser.add_argument(
        "--scene",
        type=Path,
        default=Path("build/experiments/vegetation/forest-views/temperate-views.json"),
    )
    args = parser.parse_args()
    with (
        patch.object(
            Blade, "mesh", side_effect=AssertionError("cachehit must not mesh leaves")
        ),
        patch.object(
            PlantPlan,
            "leaves",
            side_effect=AssertionError("cachehit must not place leaves"),
        ),
    ):
        result = measure(args.atlas, args.scene)
    result |= {
        "fine_organ_calls_forbidden": True,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", str(Path(__file__).parent)],
                text=True,
            ).strip()
        ),
    }
    (args.atlas / "forest-cache.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
