"""Measure actual mature near/window errors and their stand-camera pixel demand."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from conifer_whole_products import load_catalog
from conifer_window_comparison import compare_sources
from conifer_window_render import tree_bounds
from mature_forest_plan import site_angles
from mature_window_sources import physical_shoot
from outshine_ref.vegetation import NeedledPlant


def screen_extents(source, sites, case, axis, vertices):
    eye = np.asarray(source["camera"], float)
    target = np.asarray(source["target"], float)
    forward = target - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, (0, 0, 1))
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    camera = np.column_stack((right, up, forward))
    focal = source["resolution"][1] / (2 * np.tan(np.deg2rad(source["fov_y"]) / 2))
    lo, hi = vertices.min(axis=0), vertices.max(axis=0)
    corners = np.array(np.meshgrid(*zip(lo, hi))).reshape(3, -1).T
    corners = corners @ axis.frame.T + axis.origin
    selected = (sites["kinds"] == case["kind"]) & (sites["variants"] == case["variant"])
    angles = site_angles(sites)[selected]
    spans = []
    for position, angle in zip(sites["positions"][selected], angles, strict=True):
        c, s = np.cos(angle), np.sin(angle)
        rotation = np.array(((c, -s, 0), (s, c, 0), (0, 0, 1)))
        view = (corners @ rotation.T + position - eye) @ camera
        if np.any(view[:, 2] <= 0):
            raise ValueError("control shoot crosses the camera plane")
        pixels = focal * view[:, :2] / view[:, 2, None]
        spans.append(float(np.ptp(pixels, axis=0).max()))
    return {
        "sites": len(spans),
        "max_axis_span_px_min_median_max": np.quantile(spans, (0, 0.5, 1)).tolist(),
        "scope": "projected bounds of requested physical shoot at every matching original site; same vertical-FOV camera, no visibility/occlusion claim",
    }


def compare(products, scenario, output):
    began = time.perf_counter()
    scene = json.loads(scenario.read_text())
    with np.load(scenario.parent / scene["sites"], allow_pickle=False) as stored:
        sites = {k: stored[k] for k in stored.files}
    cases = json.loads(products.read_text())
    inputs = [products, scenario, scenario.parent / scene["sites"]]
    banks_by_species = {}
    rows = []
    for case in cases:
        directory = products.parent / case["product"]
        plant = NeedledPlant.from_recipe(
            json.loads((directory / "plant-recipe.json").read_text())
        )
        assert plant.identity == case["identity"]
        species = case["species"]
        if species not in banks_by_species:
            catalog = products.parent / "catalogs" / species / "catalog"
            _, banks_by_species[species] = load_catalog(catalog)
            inputs.extend(catalog.iterdir())
        banks = banks_by_species[species]
        inputs.extend((directory / "plant-recipe.json", directory / "windows.npz"))
        with np.load(directory / "windows.npz", allow_pickle=False) as stored:
            windows = {k: stored[k] for k in stored.files}
        for choice in (0, 1, 2):
            path = tuple(
                (0, level.branches // 2, level.branches - 1)[choice]
                for level in plant.wood.levels[1:]
            )
            source, wood, axis = physical_shoot(
                plant, path, species, output / "physical" / case["key"] / str(choice)
            )
            selected = windows["axis_ids"] == axis.id
            near = {k: v[selected] for k, v in windows.items()}
            near["origins"] = (near["origins"] - axis.origin) @ axis.frame
            near["frames"] = np.einsum("ji,njk->nik", axis.frame, near["frames"])
            count = sum(kind == "leaf" for kind, _ in source["ids"])
            assert int(near["needle_counts"].sum()) == count
            first = int(near["first_needle_ids"].min())
            np.testing.assert_array_equal(
                sorted(identity for kind, identity in source["ids"] if kind == "leaf"),
                np.arange(first, first + count, dtype=np.uint64),
            )
            bounds = np.concatenate(
                (source["vertices"], tree_bounds(near, banks, wood))
            )
            views = compare_sources(
                source, near, banks, wood, bounds, reductions=(1, 4, 16, 32, 64)
            )
            rows.append(
                {
                    "key": case["key"],
                    "identity": plant.identity,
                    "axis_path": path,
                    "needles": count,
                    "first_needle_id": first,
                    "windows": len(near["origins"]),
                    "screen": screen_extents(
                        scene, sites, case, axis, source["vertices"]
                    ),
                    "input_sha256": source["input_sha256"],
                    "directions": views,
                }
            )
            print(case["key"], path, "matched near/window control complete", flush=True)
    result = {
        "scope": "three actual requested shoots per mature form; same cached closed needle meshes/materials, wood axis and shared bounds; fixed unlit views; errors measured without approval",
        "views_per_shoot": sorted({len(row["directions"]) for row in rows}),
        "elapsed_s": time.perf_counter() - began,
        "shoots": rows,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", "test/experiments/vegetation"]
            )
        ),
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs
        },
        "implementation_sha256": {
            name: hashlib.sha256(
                (Path(__file__).parent / name).read_bytes()
            ).hexdigest()
            for name in (
                "mature_window_comparison.py",
                "mature_window_sources.py",
                "conifer_window_comparison.py",
                "needle_group_source.py",
            )
        },
    }
    (output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", required=True, type=Path)
    parser.add_argument("--scenario", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    compare(args.products, args.scenario, args.output)
