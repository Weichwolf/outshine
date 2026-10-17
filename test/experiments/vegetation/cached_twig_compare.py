"""Matched real cached twig needles and their finite statistical window products."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from cached_twig_source import local_windows, physical_axis, select_axes
from conifer_window_catalog import prototypes
from conifer_window_comparison import matched_images
from conifer_window_render import tree_bounds
from foliage_selection import directions
from leaf_sheet import average_blocks
from outshine_ref.vegetation import Needle, NeedledPlant
from parent_profile_graph import ParentProfileGraph
from window_portrait_source import source


def provenance():
    root = Path(__file__).resolve().parents[3]
    names = (
        "cached_twig_compare.py",
        "cached_twig_source.py",
        "conifer_window_comparison.py",
        "conifer_window_catalog.py",
        "conifer_window_render.py",
        "leaf_sheet.py",
        "textured_raster.py",
        "raster_batch.py",
    )
    files = [Path(__file__).with_name(n) for n in names]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, files)], cwd=root
    ):
        raise ValueError("freeze the comparison implementation before measuring")
    return {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True
        ).strip(),
        "implementation_sha256": {
            str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in files
        },
    }


def metrics(reference, candidate, pixel_area):
    result = {}
    for name, channel in (("all", 3), ("needle", 7)):
        a, b = reference[..., channel], candidate[..., channel]
        area = float(a.sum())
        if not area > 0:
            raise ValueError("positive physical source coverage required")
        result[name] = {
            "reference_area_m2": area * pixel_area,
            "candidate_area_m2": float(b.sum()) * pixel_area,
            "relative_area_error": float(b.sum() / area - 1),
            "l1_by_size": {
                str(a.shape[0] // f): float(
                    abs(average_blocks(a, f) - average_blocks(b, f)).sum()
                    / max(average_blocks(a, f).sum(), 1e-20)
                )
                for f in (1, 4, 16, 32)
            },
        }
    return result


def reference_modules():
    return {
        k: {
            "path": v.__file__,
            "sha256": hashlib.sha256(Path(v.__file__).read_bytes()).hexdigest(),
        }
        for k, v in sys.modules.items()
        if k.startswith("outshine_ref")
        and getattr(v, "__file__", None)
        and Path(v.__file__).suffix == ".py"
    }


def compare(product, catalog, models, output, *, size=128, samples=4):
    if size != 128 or samples not in (4, 8):
        raise ValueError(
            "explicit128square comparison with4/8subpixel samples required"
        )
    proof = provenance()
    output.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    graph, axes, windows, banks, inputs = source(
        product, product / "execution", catalog
    )
    physical = json.loads((product / "plant-recipe.json").read_text())[
        "physical_source"
    ]
    scene = json.loads((models / "shoot.json").read_text())
    meshes, model_paths = prototypes(models, scene)
    inputs["files"].update(
        {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in model_paths + [models / "shoot.json"]
        }
    )
    requests = select_axes(axes)
    wood = {
        "vertices": np.empty((0, 3)),
        "faces": np.empty((0, 3), np.int32),
        "normals": np.empty((0, 3, 3)),
    }
    cases = []
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
        patch.object(Needle, "mesh", side_effect=AssertionError("new prototype")),
    ):
        for index in requests:
            began = time.perf_counter()
            geometry, detail, facts = physical_axis(
                graph, axes, physical, meshes, index
            )
            near = local_windows(axes, windows, index)
            bounds = tree_bounds(near, banks, geometry)
            directory = output / str(index)
            directory.mkdir()
            rows, real, proxy = [], [], []
            for direction in directions(17):
                a, b, pixel_area = matched_images(
                    geometry, near, banks, wood, bounds, direction, size, samples
                )
                rows.append(
                    {
                        "direction": direction.tolist(),
                        "pixel_area_m2": pixel_area,
                        **metrics(a, b, pixel_area),
                    }
                )
                real.append(a[..., :4])
                proxy.append(b[..., :4])
            np.savez_compressed(
                directory / "views.npz",
                physical=np.array(real),
                windows=np.array(proxy),
            )
            facts |= {
                "windows": len(near["origins"]),
                "tail_weight": float(near["weights"][-1]),
                "views": rows,
                "seconds": time.perf_counter() - began,
                "source_array_sha256": {
                    k: hashlib.sha256(v.tobytes()).hexdigest()
                    for k, v in geometry.items()
                },
                "detail_array_sha256": {
                    k: hashlib.sha256(v.tobytes()).hexdigest()
                    for k, v in detail.items()
                },
            }
            cases.append(facts)
            print(
                json.dumps(
                    {
                        "axis": index,
                        "needles": facts["needles"],
                        "tail_weight": facts["tail_weight"],
                        "median_needle_area_error": float(
                            np.median(
                                [v["needle"]["relative_area_error"] for v in rows]
                            )
                        ),
                        "seconds": facts["seconds"],
                    }
                ),
                flush=True,
            )
            (directory / "report.json").write_text(json.dumps(facts, indent=2) + "\n")
    report = proof | {
        "input": inputs,
        "cases": cases,
        "size": size,
        "subpixel_samples": samples,
        "seconds": time.perf_counter() - start,
        "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "loaded_reference_modules": reference_modules(),
        "scope": "Nine real cached twig requests only, closed physical needle/short-shoot prototypes against existing statistical windows; shared bounds/projection/filter,17unlit orthographic directions. No main wood to hide needle error, no whole near plant, lighting/native/device/LOD approval.",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("product", "catalog", "models", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--samples", type=int, default=4)
    args = parser.parse_args()
    compare(args.product, args.catalog, args.models, args.output, samples=args.samples)
