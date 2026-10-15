"""Counterexamples for depth relief and organ bands with identical source views."""

import json
import time

import matplotlib
import numpy as np
from foliage_selection import directions
from needle_group_experiment import BASE, OUTPUT, error_summary, run_provenance
from needle_group_layers import depth_layers, render_layers
from needle_group_reprojection import compare_images, reproject
from needle_group_source import source_geometry
from needle_group_views import load_views, render_view


def comparison_figure(images):
    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    figure, axes = plt.subplots(1, 4, figsize=(16, 9))
    titles = (
        "Closed 3D reference",
        "One card / 64 views",
        "16×16 depth relief",
        "Eight depth bands",
    )
    for axis, image, title in zip(axes, images, titles, strict=True):
        axis.imshow(image["rgba"][..., 3], origin="lower", cmap="gray", vmin=0, vmax=1)
        axis.set_title(title)
        axis.set_axis_off()
    figure.tight_layout()
    figure.savefig(OUTPUT / "picea-view-comparison.png", dpi=160)
    plt.close(figure)


def main():
    record = json.loads((OUTPUT / "angular-report.json").read_text())[0]
    source = source_geometry(BASE / "picea-abies")
    cache = load_views(record["cache_path"], source["identity"])
    sphere, validation = directions(64), directions(17)
    references = [render_view(source, d, 64, samples=8) for d in validation]
    indices = np.argmax(validation @ sphere.T, axis=1)
    variants = []
    for grid in (1, 4, 8, 16):
        began = time.perf_counter()
        images = [
            reproject(cache[i], source, d, grid=grid)
            for i, d in zip(indices, validation, strict=True)
        ]
        variants.append(
            {
                "relief_grid": grid,
                "selected_triangles": 2 * grid**2,
                "comparison_s": time.perf_counter() - began,
            }
            | error_summary(
                [compare_images(a, b) for a, b in zip(references, images, strict=True)]
            )
        )
    bands = []
    for count in (1, 2, 4, 8):
        began = time.perf_counter()
        products = {
            int(i): depth_layers(source, sphere[i], count) for i in set(indices)
        }
        build_s = time.perf_counter() - began
        began = time.perf_counter()
        images = [
            render_layers(products[int(i)], source, d)
            for i, d in zip(indices, validation, strict=True)
        ]
        bands.append(
            {
                "depth_bands": count,
                "selected_triangles_max": 2 * count,
                "required_view_build_s": build_s,
                "comparison_s": time.perf_counter() - began,
            }
            | error_summary(
                [compare_images(a, b) for a, b in zip(references, images, strict=True)]
            )
        )
    selected = 5
    index, direction = int(indices[selected]), validation[selected]
    comparison_figure(
        (
            references[selected],
            reproject(cache[index], source, direction),
            reproject(cache[index], source, direction, grid=16),
            render_layers(products[index], source, direction),
        )
    )
    result = {
        "plan_identity": source["identity"],
        "axis_path": source["axis_path"],
        "control_commit": record["commit"],
        "input_sha256": source["input_sha256"],
        "reference_view_size": 64,
        "reference_sample_rows": 8,
        "bake_view_count": 64,
        "bake_sample_rows": 4,
        "relief": variants,
        "organ_depth_bands": bands,
        "diagnostic_direction": direction.tolist(),
        "decision": "Keep the closed whole-tile oracle; these averaged-depth alternatives do not approve close shoot coverage. Reassess representation before further repair iterations.",
        "scope": "Unlit first-hit coverage/depth only. Bands are organ partitions, not a complete LDI. No shader, shadow, wind, whole-tile, GPU or runtime approval.",
    } | run_provenance()
    (OUTPUT / "picea-depth-report.json").write_text(json.dumps(result, indent=2) + "\n")
    for kind in (variants, bands):
        print(
            [
                (r["max_absolute_area_error"], r["max_coverage_l1_relative"])
                for r in kind
            ]
        )


if __name__ == "__main__":
    main()
