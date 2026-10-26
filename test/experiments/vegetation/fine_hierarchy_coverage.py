"""Shared unlit leaf-coverage projections, separate from area and lit portraits."""

import argparse
import json
import resource
import time
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from conifer_window_render import tree_bounds, window_fragments
from leaf_sheet import average_blocks
from matplotlib.path import Path as PolygonPath
from plant_atlas import projection
from scipy.spatial import ConvexHull
from textured_raster import composite
from window_portrait_source import source


def common_envelope(windows, frame, lower, upper, size):
    points = np.concatenate([w["origins"] @ frame for w in windows])[:, :2]
    hull = ConvexHull(points)
    polygon = PolygonPath(points[hull.vertices])
    y, x = np.mgrid[:size, :size]
    coordinates = lower + (np.column_stack((x.ravel(), y.ravel())) + 0.5) / size * (
        upper - lower
    )
    return polygon.contains_points(coordinates).reshape(size, size)


def project(windows, banks, direction, frame, lower, upper, size=512, samples=2):
    began = time.perf_counter()
    pixel, depth, values = window_fragments(
        windows, banks, direction, frame, lower, upper, size * samples
    )
    image = average_blocks(composite(pixel, depth, values, size * samples), samples)
    leaf = image[..., 7]
    if not np.isfinite(image).all() or leaf.min() < -1e-8 or leaf.max() > 1 + 1e-8:
        raise ValueError("finite composited leaf-weighted coverage required")
    return image, {"fragments": len(pixel), "seconds": time.perf_counter() - began}


def compare(control, candidate, catalog, output):
    started = time.perf_counter()
    output.mkdir(parents=True, exist_ok=False)
    products = [source(p, p / "execution", catalog) for p in (control, candidate)]
    windows = [p[2] for p in products]
    corners = np.concatenate(
        [tree_bounds(p[2], p[3], {"vertices": p[1]["origins"]}) for p in products]
    )
    rows = []
    for view, angle in enumerate((15, 75, 135)):
        a = np.deg2rad(angle)
        direction = np.array([np.cos(a), np.sin(a), 0.0])
        frame, lower, upper, _ = projection(corners, direction)
        mask = common_envelope(windows, frame, lower, upper, 512)
        pixel_area = float(np.prod(upper - lower) / 512**2)
        for case, product in enumerate(products):
            image, cost = project(
                product[2], product[3], direction, frame, lower, upper
            )
            leaf = image[..., 7]
            record = {
                "case": case,
                "azimuth_degrees": angle,
                **cost,
                "leaf_coverage_m2": float(leaf.sum() * pixel_area),
                "common_envelope_m2": float(mask.sum() * pixel_area),
                "mean_leaf_coverage_in_common_envelope": float(leaf[mask].mean()),
                "fraction_envelope_leaf_below_0_05": float((leaf[mask] < 0.05).mean()),
                "lower": lower.tolist(),
                "upper": upper.tolist(),
                "frame": frame.tolist(),
            }
            rows.append(record)
            np.savez_compressed(
                output / f"case-{case}-view-{view}.npz", image=image, common_mask=mask
            )
            print(json.dumps(record), flush=True)
    report = {
        "cases": rows,
        "product_provenance": [p[4] for p in products],
        "resolution": [512, 512],
        "subsamples_per_axis": 2,
        "scope": "Six same-frame unlit statistical leaf-weighted projections, three horizontal views. Common convex centre envelope, not botanical crown outline/gap-fraction/effective-LAI estimate or all-view/temporal/LOD approval.",
    }
    plot(output, output / "coverage.png", report)
    report |= {
        "process_s": time.perf_counter() - started,
        "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
    }
    (output / "coverage.json").write_text(json.dumps(report, indent=2) + "\n")


def plot(directory, output, report=None):
    if report is None:
        report = json.loads((directory / "coverage.json").read_text())
    fig, axes = plt.subplots(2, 3, figsize=(16, 9), dpi=160)
    for view, angle in enumerate((15, 75, 135)):
        for case in (0, 1):
            row = next(
                r
                for r in report["cases"]
                if r["case"] == case and r["azimuth_degrees"] == angle
            )
            with np.load(
                directory / f"case-{case}-view-{view}.npz", allow_pickle=False
            ) as stored:
                leaf, mask = stored["image"][..., 7], stored["common_mask"]
            lower, upper = row["lower"], row["upper"]
            axes[case, view].imshow(
                np.flipud(np.clip(leaf, 0, 1)),
                cmap="gray_r",
                vmin=0,
                vmax=1,
                extent=(lower[0], upper[0], lower[1], upper[1]),
            )
            axes[case, view].set_title(
                f"{'Control' if case == 0 else 'Branching end twigs'} | azimuth {angle}°\nmean leaf {leaf[mask].mean():.3f}; weak/empty {(leaf[mask] < 0.05).mean():.1%}",
                fontsize=10,
            )
            axes[case, view].axis("off")
    fig.suptitle(
        "Picea abies | leaf-weighted proxy coverage, shared512² frame /2×2subsamples",
        fontsize=16,
    )
    fig.text(
        0.5,
        0.02,
        "Common convex envelope of both window-centre projections. Main wood excluded; canonical short wood occlusion remains. Not observed LAI or a lit rendering.",
        ha="center",
        fontsize=9,
    )
    fig.tight_layout(rect=(0.01, 0.05, 0.99, 0.94))
    fig.savefig(output)
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("control", "candidate", "catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    compare(args.control, args.candidate, args.catalog, args.output)
