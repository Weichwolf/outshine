"""Actual physical grass union/cost at increasing authored root densities."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from ground_cover_scene import tuft_geometry, tuft_poses
from outshine_ref.vegetation import GroundCoverPlan
from outshine_ref.vegetation.shoot_plan import normal_frame
from raster_batch import fragments


def raster_sites():
    rng, step = np.random.default_rng(113), 0.65
    x, y = np.meshgrid(
        np.arange(-2 + step / 2, 2, step), np.arange(-2 + step / 2, 2, step)
    )
    xy = np.c_[x.ravel(), y.ravel()] + rng.uniform(-0.4 * step, 0.4 * step, (x.size, 2))
    variants, marks = rng.integers(0, 8, len(xy)), np.empty((len(xy), 3))
    for index in range(8):
        selected = variants == index
        marks[selected, 0] = rng.uniform(-0.5, 0.5, selected.sum()) % 1
        marks[selected, 1] = rng.random(selected.sum())
        marks[selected, 2] = (index + 0.5) / 8
    return {
        "ids": np.c_[np.floor(xy).astype(np.int64), np.arange(len(xy))],
        "xy": xy,
        "marks": marks,
    }


def projected_cover(vertices, faces, direction, size):
    frame = normal_frame(np.array(direction, float), np.array((0, 0, 1)))
    basis = frame[:, (0, 2, 1)]
    ground = np.array([[-2, -2, 0], [2, -2, 0], [2, 2, 0], [-2, 2, 0]]) @ basis
    projected = vertices @ basis
    lower, upper = ground[:, :2].min(0) - 0.01, ground[:, :2].max(0) + 0.01
    mask, cover = np.zeros(size * size, bool), np.zeros(size * size, bool)
    for _, _, index, _ in fragments(
        ground, np.array([[0, 1, 2], [0, 2, 3]]), lower, upper, size
    ):
        mask[index] = True
    for _, _, index, _ in fragments(projected, faces, lower, upper, size):
        cover[index] = True
    return {
        "pixels": size,
        "ground_pixels": int(mask.sum()),
        "covered_fraction": float((cover & mask).sum() / mask.sum()),
    }


def scalar_sites(plan, lower, upper):
    """Independent scalar expansion, exact same per-cell PCG64 sequence."""
    summary = plan.summary(lower, upper)
    ids, xy, marks = [], [], []
    for cell, count in zip(summary["cells"], summary["counts"], strict=True):
        rng = plan._random(cell, b"sites")
        for ordinal in range(count):
            u = rng.random(5)
            point = cell * plan.cell_m + u[:2] * plan.cell_m
            if np.all(point >= lower) and np.all(point < upper):
                ids.append((*cell, ordinal))
                xy.append(point)
                marks.append(u[2:])
    return {
        "ids": np.asarray(ids, np.int64).reshape(-1, 3),
        "xy": np.asarray(xy).reshape(-1, 2),
        "marks": np.asarray(marks).reshape(-1, 3),
    }


def benchmark():
    plan = GroundCoverPlan(113, 100)
    rounds = {"batch_s": [], "scalar_s": []}
    for trial in range(4):
        order = ("batch_s", "scalar_s") if trial % 2 == 0 else ("scalar_s", "batch_s")
        products = {}
        for name in order:
            begin = time.perf_counter()
            products[name] = (
                plan.sites((-14, -14), (14, 14))
                if name == "batch_s"
                else scalar_sites(plan, (-14, -14), (14, 14))
            )
            rounds[name].append(time.perf_counter() - begin)
        for key in products["batch_s"]:
            np.testing.assert_array_equal(
                products["batch_s"][key], products["scalar_s"][key]
            )
    return {
        **rounds,
        "roots": len(products["batch_s"]["xy"]),
        "returned_bytes": sum(v.nbytes for v in products["batch_s"].values()),
        "scope": "Same counts, positions, IDs and marks, exact scalar/batch replay; no mesh construction.",
    }


def run(output):
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    for density in (None, 1 / 0.65**2, 25, 75, 150, 300):
        plan = GroundCoverPlan(113, density) if density is not None else None
        began = time.perf_counter()
        sites = plan.sites((-2, -2), (2, 2)) if plan else raster_sites()
        site_s = time.perf_counter() - began
        poses = tuft_poses(sites)
        vertices, faces = tuft_geometry(poses)
        views = [
            {
                "direction": direction,
                "coverage": [
                    projected_cover(vertices, faces, direction, size)
                    for size in (256, 512)
                ],
            }
            for direction in ((0, 0, 1), (14, -19, 11.7))
        ]
        row = {
            "density_m2": density,
            "roots": len(sites["xy"]),
            "method": "poisson" if plan else "legacy-raster",
            "plan": plan.recipe() if plan else None,
            "site_s": site_s,
            "triangles": len(faces),
            "expanded_reference_bytes": vertices.nbytes + faces.nbytes,
            "views": views,
        }
        rows.append(row)
        label = f"{density:.3f}" if plan else "raster"
        np.savez_compressed(output / f"sites-{label}.npz", **sites)
        print(json.dumps(row), flush=True)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cases": rows,
        "benchmark": benchmark(),
        "scope": "Generic grass, authored Poisson root densities, actual three-segment blades. Union within the projected4x4m ground polygon. Sampled views, not ecological/LOD/native admission.",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
