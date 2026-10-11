"""Check physical organ geometry and create declared near-detail render fixtures."""

import argparse
import gc
import json
import resource
import subprocess
import time
from dataclasses import replace
from pathlib import Path

import numpy as np
from axis_surface import axis_mesh
from blade_surface import Blade
from curve_frame import arc_frame
from shoot_plan import Shoot, normal_frame

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
UPRIGHT = np.array(((1, 0, 0), (0, 0, -1), (0, 1, 0)), dtype=float)
GRASS = Blade(
    length=0.45,
    width=0.009,
    widest=0.1,
    lobes=0,
    lobe_depth=0,
    base_fill=0.7,
    bend=1.15,
    fold=0.25,
    fold_power=1,
    twist=0,
    petiole=0,
)


def triangle_areas(mesh):
    points = mesh["vertices"][mesh["faces"]]
    cross = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    return np.linalg.norm(cross, axis=1) / 2, cross


def check_blades():
    oak = Blade(lobes=6)
    cases = [
        oak,
        replace(oak, fold_power=1),
        GRASS,
        replace(oak, bend=0, twist=0, fold=0),
        replace(oak, bend=-1.3, fold=-0.25, twist=1.4),
        replace(oak, base_fill=0, petiole=0),
        replace(oak, petiole=0.018),
        replace(oak, bend=2.4, twist=-0.7),
    ]
    results = []
    t = np.linspace(0.08, 0.92, 47)
    epsilon = 1e-6
    for blade in cases:
        for s, side in ((0.37, 1), (-0.61, -1)):
            position, normal, density = blade.evaluate(t, s, side)
            dt = (
                blade.evaluate(t + epsilon, s, side)[0]
                - blade.evaluate(t - epsilon, s, side)[0]
            ) / (2 * epsilon)
            ds = (
                blade.evaluate(t, s + 0.01, side)[0]
                - blade.evaluate(t, s - 0.01, side)[0]
            ) / 0.02
            measured = np.cross(ds, dt)
            measured_density = np.linalg.norm(measured, axis=-1)
            np.testing.assert_allclose(
                normal, measured / measured_density[:, None], atol=2e-8
            )
            np.testing.assert_allclose(density, measured_density, rtol=2e-8)
            assert np.isfinite(position).all()
        mesh = blade.mesh(512, across=8)
        areas, _ = triangle_areas(mesh)
        assert np.min(areas) > 0 and np.isfinite(mesh["normals"]).all()
        np.testing.assert_allclose(
            np.linalg.norm(mesh["normals"], axis=1), 1, atol=1e-12
        )
        area = blade.area(256)
        np.testing.assert_allclose(blade.area(128), area, rtol=1e-8)
        np.testing.assert_allclose(areas.sum(), area, rtol=3e-4)
        coarse = blade.mesh(16, across=2)
        fine = blade.mesh(32, across=4)
        lookup = {
            tuple(uv): p
            for uv, p in zip(fine["parameters"], fine["vertices"], strict=True)
        }
        for uv, p in zip(coarse["parameters"], coarse["vertices"], strict=True):
            np.testing.assert_array_equal(p, lookup[tuple(uv)])
        if blade.petiole and blade.base_fill:
            _, n, _ = blade.evaluate(0, 0.5, 1, petiole_side=True)
            slope = blade.fold_power * 0.5 ** (blade.fold_power - 1)
            np.testing.assert_allclose(
                n,
                np.array((-blade.fold * slope, 0, 1))
                / np.sqrt(1 + (blade.fold * slope) ** 2),
                atol=1e-12,
            )
        if blade.fold_power == 2:
            np.testing.assert_array_equal(
                blade.evaluate(t, 0, -1)[1], blade.evaluate(t, 0, 1)[1]
            )
            scalar = replace(blade, bend=0, twist=0, petiole=0.004, lobes=0)
            width = scalar.width_profile(0.0)[0]
            _, _, density = scalar.evaluate(-0.002 / scalar.length, 0.5, 1)
            np.testing.assert_allclose(
                density, width * scalar.length * np.sqrt(1 + scalar.fold**2), rtol=1e-12
            )
        results.append(
            {
                "area_m2": area,
                "mesh_area_relative_error": abs(areas.sum() / area - 1),
                "fine_triangles": len(mesh["faces"]),
            }
        )
    for bend in (0, 1e-12, -0.7, np.pi):
        centre, tangent, lateral, normal = arc_frame(0.48, bend, 0.7, t)
        frames = np.stack((lateral, tangent, normal), axis=-1)
        np.testing.assert_allclose(
            np.einsum("nji,njk->nik", frames, frames),
            np.broadcast_to(np.eye(3), frames.shape),
            atol=1e-12,
        )
        derivative = (
            arc_frame(0.48, bend, 0.7, t + epsilon)[0]
            - arc_frame(0.48, bend, 0.7, t - epsilon)[0]
        ) / (2 * epsilon)
        np.testing.assert_allclose(derivative, 0.48 * tangent, atol=1e-9)
        assert np.isfinite(centre).all()
    for invalid in (
        {"width": 0},
        {"widest": 1},
        {"length": np.nan},
        {"lobes": 0.5},
        {"bend": 4},
        {"lobe_depth": 1},
        {"fold_power": 3},
    ):
        try:
            Blade(**invalid)
        except ValueError:
            continue
        raise AssertionError(f"accepted invalid blade: {invalid}")
    return results


def check_axes_and_groups():
    for bend in (0, 0.3, -1.2, np.pi):
        for segments, sides in ((2, 3), (8, 8), (24, 12)):
            mesh = axis_mesh(0.48, bend, 0.004, 0.001, segments, sides)
            areas, cross = triangle_areas(mesh)
            assert np.min(areas) > 0
            normals = mesh["normals"].reshape(-1, 3, 3)
            assert np.all(np.sum(cross * normals.mean(axis=1), axis=1) > 0)
            edges = np.sort(
                np.concatenate(
                    [
                        mesh["faces"][:, [0, 1]],
                        mesh["faces"][:, [1, 2]],
                        mesh["faces"][:, [2, 0]],
                    ]
                ),
                axis=1,
            )
            _, counts = np.unique(edges, axis=0, return_counts=True)
            np.testing.assert_array_equal(counts, 2)
    counts = {}
    for style in ("spiral", "distichous", "opposite", "decussate", "whorled"):
        shoot = Shoot(arrangement=style)
        summary = shoot.summary()
        assert "_placements" not in shoot.__dict__
        assert shoot.detail(0) is None and "_placements" not in shoot.__dict__
        poses = shoot.detail(shoot.count)
        assert shoot.detail(shoot.count) is poses
        repeat = Shoot(arrangement=style).detail(shoot.count)
        for key in poses:
            np.testing.assert_array_equal(poses[key], repeat[key])
        np.testing.assert_allclose(np.linalg.det(poses["frames"]), 1, atol=1e-12)
        root = shoot.blade.evaluate(-shoot.blade.petiole / shoot.blade.length, 0, 1)[0]
        attached = poses["origins"] + np.einsum("nij,j->ni", poses["frames"], root)
        np.testing.assert_allclose(attached, poses["nodes"], atol=1e-15)
        mesh = shoot.blade.mesh(32)
        vertices = (
            np.einsum("nij,kj->nki", poses["frames"], mesh["vertices"])
            + poses["origins"][:, None, :]
        )
        assert (
            np.max(np.linalg.norm(vertices, axis=-1)) <= summary["root_bound_radius_m"]
        )
        np.testing.assert_array_equal(poses["ids"], np.arange(summary["leaves"]))
        groups = poses["stem_parameter"][:: shoot.per_group]
        np.testing.assert_allclose(
            np.diff(groups) * shoot.length, -shoot.internode, atol=1e-15
        )
        counts[style] = summary["leaves"]
    return counts


def storage_comparison():
    mesh = Blade(lobes=6).mesh(32)
    poses = Shoot().detail(100)
    results = []
    for count in (1000, 10000):
        selection = np.arange(count) % len(poses["ids"])
        origins, frames = poses["origins"][selection], poses["frames"][selection]
        timings = {"shared": [], "expanded": []}
        sizes = {}
        for method, measurements in timings.items():
            for _ in range(3):
                gc.collect()
                began = time.perf_counter()
                if method == "shared":
                    product = {
                        "origins": origins.copy(),
                        "frames": frames.copy(),
                        **mesh,
                    }
                else:
                    product = {
                        "vertices": (
                            np.einsum("nij,kj->nki", frames, mesh["vertices"])
                            + origins[:, None, :]
                        ).reshape(-1, 3),
                        "normals": np.einsum(
                            "nij,kj->nki", frames, mesh["normals"]
                        ).reshape(-1, 3),
                        "faces": (
                            mesh["faces"][None, :, :]
                            + (
                                np.arange(count, dtype=np.int32) * len(mesh["vertices"])
                            )[:, None, None]
                        ).reshape(-1, 3),
                        "parameters": np.tile(mesh["parameters"], (count, 1)),
                    }
                measurements.append((time.perf_counter() - began) * 1000)
                if method == "expanded":
                    for i in (0, count - 1):
                        actual = product["vertices"][
                            i * len(mesh["vertices"]) : (i + 1) * len(mesh["vertices"])
                        ]
                        np.testing.assert_allclose(
                            actual,
                            mesh["vertices"] @ frames[i].T + origins[i],
                            atol=1e-15,
                        )
                    del actual
                sizes[method] = sum(a.nbytes for a in product.values())
                del product
        results.append(
            {
                "leaves": count,
                "bytes": sizes,
                "median_build_ms": {k: float(np.median(v)) for k, v in timings.items()},
                "build_samples_ms": timings,
                "mesh_segments": 32,
                "mesh_across": 1,
                "triangles_per_leaf": len(mesh["faces"]),
                "placement_planning": "common input excluded",
                "timing_scope": "geometry assembly; common mesh and placements excluded",
                "same_geometry": True,
            }
        )
    return results


def write_product(output, name, mesh, origins, frames, surface):
    np.savez_compressed(output / f"{name}-mesh.npz", **mesh)
    np.savez_compressed(output / f"{name}-poses.npz", origins=origins, frames=frames)
    return {
        "mesh": f"{name}-mesh.npz",
        "placements": f"{name}-poses.npz",
        "surface": surface,
        "triangles_per_instance": len(mesh["faces"]),
        "instances": len(origins),
    }


def make_scenes(output):
    products = []
    origins, frames, variants = [], [], []
    for index, style in enumerate(("distichous", "spiral", "decussate")):
        shoot = Shoot(seed=17 + index, bend=0.22 + 0.14 * index, arrangement=style)
        offset = np.array(((index - 1) * 0.3, 0, -0.006))
        poses = shoot.detail(100)
        origins.extend(poses["origins"] @ UPRIGHT.T + offset)
        frames.extend(np.einsum("ij,njk->nik", UPRIGHT, poses["frames"]))
        variants.extend((index * 5 + poses["ids"] * 3) % 8)
        products.append(
            write_product(
                output,
                f"stem-{index}",
                axis_mesh(shoot.length, shoot.bend, 0.004, 0.0008),
                offset[None, :],
                UPRIGHT[None, :, :],
                "wood",
            )
        )
    origins, frames, variants = np.array(origins), np.array(frames), np.array(variants)
    for variant in range(8):
        blade = Blade(
            lobes=6,
            length=0.12 * (0.85 + 0.15 * variant / 7),
            width=0.06 * (0.88 + 0.12 * ((variant * 3) % 8) / 7),
            fold=0.045 + 0.085 * variant / 7,
            bend=0.18 + 0.27 * variant / 7,
            twist=(variant - 3.5) * 0.08,
        )
        mask = variants == variant
        products.append(
            write_product(
                output,
                f"oak-leaf-{variant}",
                blade.mesh(64, across=2),
                origins[mask],
                frames[mask],
                "leaf",
            )
        )
    recipes = [
        {
            "name": "shoots",
            "title": "Belaubte Triebe: wechselständig, spiralig, kreuzgegenständig",
            "products": products,
            "camera": [0.55, -1.15, 0.78],
            "target": [0, 0, 0.26],
            "fov_y": 32,
            "ground_size_m": 50,
        }
    ]
    rng = np.random.default_rng(31)
    origins, frames, variants = [], [], []
    for x in np.linspace(-0.9, 0.9, 14):
        for y in np.linspace(-0.9, 0.9, 14):
            centre = np.array((x, y, 0.0)) + np.r_[rng.uniform(-0.055, 0.055, 2), 0]
            for _ in range(9):
                angle = rng.uniform(-np.pi, np.pi)
                outward = np.array((np.cos(angle), np.sin(angle), 0.0))
                direction = np.array((0, 0, 1)) + rng.uniform(0, 0.2) * outward
                origins.append(centre + np.r_[rng.uniform(-0.025, 0.025, 2), 0])
                frames.append(normal_frame(direction, outward))
                variants.append(rng.integers(8))
    origins, frames, variants = np.array(origins), np.array(frames), np.array(variants)
    products = []
    for variant in range(8):
        blade = replace(
            GRASS,
            length=0.30 + 0.035 * variant,
            bend=0.65 + 0.12 * variant,
            twist=(variant - 3.5) * 0.035,
        )
        mask = variants == variant
        products.append(
            write_product(
                output,
                f"grass-{variant}",
                blade.mesh(24),
                origins[mask],
                frames[mask],
                "grass",
            )
        )
    recipes.append(
        {
            "name": "grass",
            "title": "Gebogene Grashalme in bodenverbundenen Horsten",
            "products": products,
            "camera": [1.65, -2.2, 1.0],
            "target": [0, 0, 0.22],
            "fov_y": 44,
            "ground_size_m": 50,
        }
    )
    for recipe in recipes:
        recipe.update(
            {
                "resolution": [1280, 720],
                "samples": 32,
                "source_commit": subprocess.check_output(
                    ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
                ).strip(),
            }
        )
        (output / f"{recipe['name']}.json").write_text(
            json.dumps(recipe, indent=2) + "\n"
        )
    return [
        {
            "name": r["name"],
            "instances": sum(p["instances"] for p in r["products"]),
            "instanced_triangles": sum(
                p["instances"] * p["triangles_per_instance"] for p in r["products"]
            ),
            "shared_meshes": len(r["products"]),
        }
        for r in recipes
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/organs")
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    blades = check_blades()
    groups = check_axes_and_groups()
    storage = storage_comparison()
    scenes = make_scenes(args.output)
    report = {
        "blades": blades,
        "phyllotaxis_counts": groups,
        "storage": storage,
        "scenes": scenes,
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "peak_scope": "whole Linux correctness/generation/comparison process, not GPU/runtime",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    str(HERE / "organ_experiment.py"),
                    str(HERE / "blade_surface.py"),
                    str(HERE / "shoot_plan.py"),
                    str(HERE / "axis_surface.py"),
                    str(HERE / "curve_frame.py"),
                ],
                cwd=ROOT,
                text=True,
            ).strip()
        ),
    }
    (args.output / "organs.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
