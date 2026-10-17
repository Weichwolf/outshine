"""Freeze two bounded real needle windows and the same explicitly requested organs."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from unittest.mock import patch

import numpy as np
from cached_twig_source import local_windows, physical_axis
from conifer_window_catalog import prototypes
from outshine_ref.vegetation import Needle, NeedledPlant
from parent_profile_graph import ParentProfileGraph
from window_portrait_source import source


def provenance():
    root = Path(__file__).resolve().parents[3]
    names = (
        "twig_light_source.py",
        "twig_light_blender.py",
        "twig_light_render.py",
        "cached_twig_source.py",
        "conifer_window_catalog.py",
        "needle_group_source.py",
        "window_portrait_blender.py",
        "window_portrait_geometry.py",
        "conifer_whole_preview.py",
        "sheet_material.py",
        "blender_preview.py",
        "preview_shadows.py",
    )
    paths = [Path(__file__).with_name(name) for name in names]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, paths)], cwd=root
    ):
        raise ValueError("freeze source, binding and rendering code before comparison")
    return {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True
        ).strip(),
        "implementation_sha256": {
            str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in paths
        },
    }


def requested_axis(axes, windows):
    orders = np.diff(axes["path_offsets"]).astype(np.int64) - 2
    choices = np.flatnonzero(
        (orders == 3) & (axes["needle_counts"] >= 544) & (axes["needle_counts"] <= 576)
    )
    if not len(choices):
        raise ValueError(
            "bounded actual terminal with two full windows and short tail required"
        )
    index = min(
        choices, key=lambda i: (abs(int(axes["needle_counts"][i]) - 547), int(i))
    )
    rows = np.flatnonzero(windows["axis_ids"] == index)
    if len(rows) != 3 or list(windows["needle_counts"][rows[:2]]) != [256, 256]:
        raise ValueError("complete first256 needles and one actual short tail required")
    if not 32 <= int(windows["needle_counts"][rows[-1]]) <= 64:
        raise ValueError("actual32to64-needle tail required")
    return int(index)


def prepare(product, catalog, models, output):
    code = provenance()
    output.mkdir(parents=True, exist_ok=False)
    graph, axes, windows, _, proof = source(product, product / "execution", catalog)
    physical = json.loads((product / "plant-recipe.json").read_text())[
        "physical_source"
    ]
    scene = json.loads((models / "shoot.json").read_text())
    meshes, paths = prototypes(models, scene)
    proof["files"].update(
        {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [models / "shoot.json", *paths]
        }
    )
    index = requested_axis(axes, windows)
    local = local_windows(axes, windows, index)
    records = []
    with (
        patch.object(
            ParentProfileGraph, "instance", side_effect=AssertionError("new growth")
        ),
        patch.object(
            ParentProfileGraph, "instances", side_effect=AssertionError("new growth")
        ),
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("whole plant")
        ),
        patch.object(
            Needle, "mesh", side_effect=AssertionError("new needle prototype")
        ),
    ):
        for name, window in (("full", 0), ("tail", len(local["origins"]) - 1)):
            first = int(
                local["first_needle_ids"][window] - axes["first_needle_ids"][index]
            )
            count = int(local["needle_counts"][window])
            geometry, _, facts = physical_axis(
                graph,
                axes,
                physical,
                meshes,
                index,
                needle_range=(first, first + count),
            )
            selected = {
                key: value[window : window + 1].copy() for key, value in local.items()
            }
            shift = (
                geometry["vertices"].min(axis=0) + geometry["vertices"].max(axis=0)
            ) / 2
            geometry["vertices"] -= shift
            selected["origins"] -= shift
            directory = output / name
            directory.mkdir()
            np.savez_compressed(directory / "physical.npz", **geometry)
            np.savez_compressed(directory / "window.npz", **selected)
            record = {
                "name": name,
                **facts,
                "origin_shift_m": shift.tolist(),
                "family": int(selected["families"][0]),
                "capacity": int(selected["capacities"][0]),
                "weight": float(selected["weights"][0]),
                "physical_bounds_m": [
                    geometry["vertices"].min(axis=0).tolist(),
                    geometry["vertices"].max(axis=0).tolist(),
                ],
            }
            (directory / "source.json").write_text(json.dumps(record, indent=2) + "\n")
            records.append(record)
    result = {
        "version": "matched-cached-twig-light-inputs-1",
        "inputs": proof,
        "producer": code,
        "requests": records,
        "scope": "Two stored owner intervals, bounded real twig detail and unchanged procedural prototypes. Statistical windows are not the same individual geometry.",
    }
    (output / "source.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"requests": records}), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("product", "catalog", "models", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    prepare(args.product, args.catalog, args.models, args.output)
