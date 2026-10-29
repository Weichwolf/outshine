"""Review complete source-bound capture support against all original primitives."""

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from capture_source_support import capture_support
from capture_support_reference import (
    corner_support,
    metadata_inputs,
    original_parts,
    original_projection,
)

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render/building-field-composition"
PLACEMENTS = ("foreground", "separated-background", "interleaved-background")


def plot_support(points, result, name, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PatchCollection
    from matplotlib.patches import Rectangle

    low, high = result["capture_lower_m"], result["capture_upper_m"]
    outside = ((points < low) | (points > high)).any(1)
    fig, axes = plt.subplots(1, 2, figsize=(13, 6), layout="constrained")
    for ax, vertical in zip(axes, (1, 2), strict=True):
        ax.scatter(
            points[~outside, 0],
            points[~outside, vertical],
            s=0.4,
            c="0.35",
            rasterized=True,
        )
        ax.scatter(
            points[outside, 0],
            points[outside, vertical],
            s=1,
            c="firebrick",
            rasterized=True,
        )
        boxes = [
            Rectangle((a[0], a[vertical]), b[0] - a[0], b[vertical] - a[vertical])
            for a, b in zip(
                result["lower_m"][~result["inside"]],
                result["upper_m"][~result["inside"]],
                strict=True,
            )
        ]
        ax.add_collection(
            PatchCollection(
                boxes,
                facecolor="none",
                edgecolor="darkorange",
                linewidth=0.4,
                alpha=0.4,
            )
        )
        ax.axvline(low[0], c="navy", ls="--")
        ax.axvline(high[0], c="navy", ls="--")
        if vertical == 1:
            ax.axhline(low[1], c="navy", ls="--")
            ax.axhline(high[1], c="navy", ls="--")
            ax.set_aspect("equal")
            ax.set_ylabel("capture up (m)")
        else:
            ax.axhline(high[2], c="navy", ls="--", label="far plane")
            ax.set_ylim(points[:, 2].min() - 80, max(points[:, 2].max(), high[2]) + 80)
            ax.set_ylabel("capture forward depth (m)")
            ax.legend()
        ax.set_xlabel("capture right (m)")
        ax.grid(alpha=0.2)
    fig.suptitle(
        name
        + ": original stored vertices; orange = retained source bounds; red = outside geometry"
    )
    fig.savefig(output / (name + ".png"), dpi=140)
    plt.close(fig)


def plan_support(group_low, group_high, lower, upper, camera, translation):
    group = capture_support(group_low, group_high, camera, translation=translation)
    if group["inside"][0]:
        inside = np.broadcast_to(group["inside"][0], (len(lower),))
        return group, inside, group, 1
    result = capture_support(lower, upper, camera, translation=translation)
    return result, result["inside"], group, 1 + len(lower)


def run(args):
    output = args.output.resolve()
    if (
        output.is_relative_to(ROOT)
        or output == Path(tempfile.gettempdir()).resolve()
        or not output.is_relative_to(Path(tempfile.gettempdir()).resolve())
    ):
        raise ValueError("Use a fresh explicit system-temp output")
    output.mkdir(parents=True, exist_ok=False)
    manifest, arrays = metadata_inputs(args.data)
    lower, upper, ids = [
        arrays[k] for k in ("source_least_m", "source_most_m", "source_ids")
    ]
    group_low, group_high = lower.min(0, keepdims=True), upper.max(0, keepdims=True)
    # The fixture derives the header once; native consumers borrow existing cohort
    # metadata. No per-source query follows a proven-positive group enclosure.
    results = {
        name: plan_support(
            group_low,
            group_high,
            lower,
            upper,
            manifest["capture_camera"],
            manifest["controlled_translations_m"][name][0],
        )
        for name in PLACEMENTS
    }
    # Fine geometry is loaded only for the opponent, after metadata decisions.
    parts = original_parts(manifest)
    records = []
    for name in PLACEMENTS:
        translation = manifest["controlled_translations_m"][name][0]
        result, source_inside, group, tested_bounds = results[name]
        corner = corner_support(
            lower, upper, manifest["capture_camera"], translation=translation
        )
        np.testing.assert_array_equal(source_inside, corner["inside"])
        if not group["inside"][0]:
            for key in result:
                np.testing.assert_array_equal(result[key], corner[key])
        actual, points = original_projection(
            parts, ids, manifest["capture_camera"], translation
        )
        clipped = actual.any(1)
        if np.any(source_inside & clipped):
            raise ValueError("Metadata admitted a clipped original source")
        outside = (result["lower_m"] < result["capture_lower_m"]) | (
            result["upper_m"] > result["capture_upper_m"]
        )
        plot_support(points, result, name, output)
        records.append(
            {
                "placement": name,
                "controlled_instance": 0 if name == "foreground" else 1,
                "sources": len(ids),
                "metadata_bounds_tested": tested_bounds,
                "cohort_box_contains_supported_group": bool(group["inside"][0]),
                "per_source_refinement_needed_after_cohort_box": not bool(
                    group["inside"][0]
                ),
                "supported_group_geometrically_contained": bool(source_inside.all()),
                "unresolved_requested_sources_remain_unqualified": bool(
                    manifest["source"]["unresolved_originals"]
                ),
                "retained_source_ordinals": ids[~source_inside].tolist(),
                "conservative_retained_sources": int((~source_inside).sum()),
                "actual_clipped_sources": int(clipped.sum()),
                "actual_axis_counts": actual.sum(0).tolist(),
                "metadata_axis_counts": outside.sum(0).tolist(),
                "conservative_extra_ordinals": ids[~source_inside & ~clipped].tolist(),
                "unknown_projection_sources": int((~result["finite"]).sum()),
                "outside_original_vertices": int(
                    (
                        (points < result["capture_lower_m"])
                        | (points > result["capture_upper_m"])
                    )
                    .any(1)
                    .sum()
                ),
                "unresolved_originals_per_instance": manifest["source"][
                    "unresolved_originals"
                ],
            }
        )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "producer_dirty": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    "test/experiments/render",
                    "lib",
                ],
                text=True,
            )
        ),
        "input_npz_sha256": manifest["npz_sha256"],
        "input_manifest_sha256": hashlib.sha256(
            (args.data / "manifest.json").read_bytes()
        ).hexdigest(),
        "source_id_namespace": manifest["source_id_namespace"],
        "metadata_array_bytes": sum(a.nbytes for a in arrays.values()),
        "camera": manifest["capture_camera"],
        "rows": records,
        "scope": "Geometric containment under fixed Double camera coefficients and Double-translation/Float32-vertex storage. All actual original triangles are an independent finite opponent; metadata path loads no fine geometry or images. Per-source flags plan a new source-bound cohort; any clipped source rejects replacement by the old whole-group field. No GPU-matrix/raster bound, sampled coverage, field-error/pose qualification, World or device-budget acceptance.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            [
                {k: v for k, v in r.items() if not k.endswith("ordinals")}
                for r in records
            ]
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
