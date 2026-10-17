"""Independent real tail windows compared with exact seeded physical subsets."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from cached_twig_compare import metrics, provenance
from cached_twig_source import local_windows, physical_axis, screen_span, select_axes
from conifer_window_catalog import prototypes
from conifer_window_comparison import matched_images
from conifer_window_render import tree_bounds
from foliage_selection import directions
from mature_tree_portrait import SCENE
from outshine_ref.vegetation import Needle, NeedledPlant
from parent_profile_graph import ParentProfileGraph
from window_portrait_source import source


def requests(axes, windows):
    excluded = set(select_axes(axes))
    result = []
    for target in (7, 15, 31, 63, 95, 127, 159, 191, 255):
        valid = np.flatnonzero(
            (windows["needle_counts"] == target)
            & (axes["needle_counts"][windows["axis_ids"]] <= 1024)
            & ~np.isin(windows["axis_ids"], list(excluded))
        )
        if not len(valid):
            raise ValueError(
                "real independent tail window required for each declared count"
            )
        index = int(valid[len(valid) // 2])
        result.append(index)
        excluded.add(int(windows["axis_ids"][index]))
    return result


def compare_window(
    graph, axes, physical, models, old_windows, new_windows, old_banks, new_banks, index
):
    axis = int(old_windows["axis_ids"][index])
    first = int(old_windows["first_needle_ids"][index])
    count = int(old_windows["needle_counts"][index])
    lo = first - int(axes["first_needle_ids"][axis])
    geometry, _, facts = physical_axis(
        graph, axes, physical, models, axis, needle_range=(lo, lo + count)
    )
    views = []
    images = []
    pair = []
    for windows in (old_windows, new_windows):
        local = local_windows(axes, windows, axis)
        mask = local["first_needle_ids"] == first
        if np.count_nonzero(mask) != 1:
            raise ValueError(
                "one identical source interval per representation required"
            )
        pair.append({k: v[mask] for k, v in local.items()})
    bounds = tree_bounds(pair[0], old_banks, geometry)
    np.testing.assert_array_equal(bounds, tree_bounds(pair[1], new_banks, geometry))
    wood = {
        "vertices": np.empty((0, 3)),
        "faces": np.empty((0, 3), np.int32),
        "normals": np.empty((0, 3, 3)),
    }
    for direction in directions(17):
        a, b, pixel_area = matched_images(
            geometry, pair[0], old_banks, wood, bounds, direction, 128, 8
        )
        reference, c, other = matched_images(
            geometry, pair[1], new_banks, wood, bounds, direction, 128, 8
        )
        np.testing.assert_array_equal(a, reference)
        assert pixel_area == other
        views.append(
            {
                "direction": direction.tolist(),
                "original": metrics(a, b, pixel_area),
                "adaptive": metrics(a, c, pixel_area),
            }
        )
        images.append(np.array((a[..., :4], b[..., :4], c[..., :4])))
    return facts | {
        "window_index": index,
        "needle_range": [lo, lo + count],
        "camera_span_px": screen_span(geometry, axes, axis, SCENE),
        "views": views,
    }, np.array(images)


def run(product, original_catalog, adaptive, capacity_catalog, models, output):
    began = time.perf_counter()
    proof = provenance()
    code = Path(__file__).resolve()
    proof["implementation_sha256"][str(code)] = hashlib.sha256(
        code.read_bytes()
    ).hexdigest()
    if subprocess.check_output(["git", "status", "--porcelain", "--", str(code)]):
        raise ValueError("freeze independent window control first")
    output.mkdir(parents=True, exist_ok=False)
    graph, axes, old, old_banks, old_proof = source(
        product, product / "execution", original_catalog
    )
    _, other, new, new_banks, new_proof = source(
        adaptive, adaptive / "execution", capacity_catalog
    )
    for key in axes:
        np.testing.assert_array_equal(axes[key], other[key])
    physical = json.loads((product / "plant-recipe.json").read_text())[
        "physical_source"
    ]
    scene = json.loads((models / "shoot.json").read_text())
    meshes, _ = prototypes(models, scene)
    rows = []
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
        for index in requests(axes, old):
            row, images = compare_window(
                graph, axes, physical, meshes, old, new, old_banks, new_banks, index
            )
            rows.append(row)
            np.savez_compressed(output / f"{index}.npz", images=images)
            print(
                json.dumps(
                    {
                        "axis": row["axis_id"],
                        "needles": row["needles"],
                        "old_l1_4px": float(
                            np.median(
                                [
                                    v["original"]["needle"]["l1_by_size"]["4"]
                                    for v in row["views"]
                                ]
                            )
                        ),
                        "new_l1_4px": float(
                            np.median(
                                [
                                    v["adaptive"]["needle"]["l1_by_size"]["4"]
                                    for v in row["views"]
                                ]
                            )
                        ),
                    }
                ),
                flush=True,
            )
    (output / "holdout.json").write_text(
        json.dumps(
            proof
            | {
                "original": old_proof,
                "adaptive": new_proof,
                "cases": rows,
                "subpixel_samples": 8,
                "seconds": time.perf_counter() - began,
                "process_peak_rss_MiB": resource.getrusage(
                    resource.RUSAGE_SELF
                ).ru_maxrss
                / 1024,
                "scope": "Nine previously unused real axes; isolated actual tail intervals7..255needles, no synthetic random recipes or whole near plant. Same bounds/directions/128square with8x8subpixels; unresolved broader anatomy/native/LOD scope.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in (
        "product",
        "original-catalog",
        "adaptive",
        "capacity-catalog",
        "models",
        "output",
    ):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    run(
        args.product,
        args.original_catalog,
        args.adaptive,
        args.capacity_catalog,
        args.models,
        args.output,
    )
