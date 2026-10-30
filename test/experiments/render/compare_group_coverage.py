"""Compare pixel-integrated and four-point coverage on the existing original group."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from compare_directed_groups import retained_subset, transformed
from compare_vertex_cluster_meshes import ready_source, setup_view
from full_view_reference import load
from process_memory import peak_rss_bytes
from triangle_coverage_reference import four_samples, front_source, integrate
from triangle_image_reference import average_samples, intersect_source


def read(path):
    with np.load(path, allow_pickle=False) as archive:
        return dict(archive)


def render(areas, sampled, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(3, 3, figsize=(15, 6), constrained_layout=True)
    for column, name in enumerate(areas):
        for row, (values, label) in enumerate(
            (
                (areas[name], "area integral"),
                (sampled[name], "2x2 samples"),
                (abs(areas[name] - sampled[name]), "absolute sampling error"),
            )
        ):
            axes[row, column].imshow(
                values,
                vmin=0,
                vmax=1,
                interpolation="nearest",
                cmap="magma" if row == 2 else "gray",
            )
            axes[row, column].set_title(name + " / " + label)
    figure.suptitle(
        "Existing original group / moving-023 / 1280x720; opaque coverage reference only"
    )
    figure.savefig(output / "coverage.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    products = {n: read(args.product / (n + ".npz")) for n in ("control", "centre")}
    arrays = products["control"]
    if any(
        not np.array_equal(products["centre"][key], arrays[key])
        for key in ("basis", "anchor", "accepted_sources", "needs_sources")
    ):
        raise ValueError("common product frame and complete source partition required")
    native, document, context, receipt = load(args.input)
    original = transformed(native, arrays["basis"], arrays["anchor"])
    retained = retained_subset(original, arrays["needs_sources"])
    sources = {"original": original} | {
        n: ready_source(a, retained) for n, a in products.items()
    }
    if not np.array_equal(
        np.sort(np.r_[arrays["accepted_sources"], arrays["needs_sources"]]),
        np.unique(native["binding"][:, 1]),
    ):
        raise ValueError("all original source identities must remain accounted for")
    view, ground, box, origins, directions = setup_view(
        {"positions": np.concatenate([s["positions"] for s in sources.values()])},
        arrays,
        document,
        context,
        {"direction": [0.45, -0.5, 0.7]},
    )
    areas, sampled, reports, silhouettes = {}, {}, {}, {}
    for name, source in sources.items():
        if source["positions"][:, 2].min() <= ground or view[0][2] <= ground:
            raise ValueError(
                "this isolated-group comparison requires unoccluded floor support"
            )
        wall, cpu = time.perf_counter(), time.process_time()
        oriented, orientation_bytes = front_source(source, view[0])
        areas[name], silhouettes[name], work = integrate(oriented, view, box)
        work.update(
            original_input_triangles=len(source["indices"]),
            orientation_coordinate_bytes=orientation_bytes,
        )
        sampled[name] = four_samples(silhouettes[name], areas[name].shape)
        depth, _ = intersect_source(oriented, origins, directions)
        height, width = areas[name].shape
        ray_samples = average_samples(
            np.isfinite(depth).astype(float), width, height, 2
        )[..., 0]
        if not np.array_equal(sampled[name], ray_samples):
            raise ValueError(
                "projected point mask differs from existing opaque ray model"
            )
        reports[name] = {
            "work": work,
            "coverage_sum_px": float(areas[name].sum()),
            "sample_sum_px": float(sampled[name].sum()),
            "sampling_l1_px": float(abs(areas[name] - sampled[name]).sum()),
            "sampling_max_px": float(abs(areas[name] - sampled[name]).max()),
            "ray_sample_pixels_equal": True,
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
        }
        if name != "original":
            reports[name]["against_original"] = {
                "integrated_l1_px": float(abs(areas[name] - areas["original"]).sum()),
                "four_sample_l1_px": float(
                    abs(sampled[name] - sampled["original"]).sum()
                ),
                "integrated_max_pixel_coverage_error": float(
                    abs(areas[name] - areas["original"]).max()
                ),
                "unfiltered_symmetric_difference_area_px": float(
                    shapely.symmetric_difference(
                        silhouettes[name], silhouettes["original"]
                    ).area
                ),
            }
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_receipt": receipt,
        "product_sha256": {
            n: hashlib.sha256((args.product / (n + ".npz")).read_bytes()).hexdigest()
            for n in products
        },
        "view": "moving-023",
        "resolution": [int(view[3]), int(view[4])],
        "roi": box.tolist(),
        "reports": reports,
        "process_peak_rss_before_plot_bytes": peak_rss_bytes(),
        "scope": "Isolated front-facing opaque group silhouette with pixel box filter; GEOS Double polygon arithmetic, not exact predicates or certified bounds. Existing point-ray model agrees only on this finite input. No native/MSAA/depth/PBR/C/T/S, world or device admission. GEOS copies/allocations and cumulative allocation count unmeasured; process RSS includes all alternatives and original inputs.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez(
        args.output / "coverage.npz",
        **{k + "__area": v for k, v in areas.items()},
        **{k + "__sample": v for k, v in sampled.items()},
    )
    if args.render:
        render(areas, sampled, args.output)


if __name__ == "__main__":
    main()
