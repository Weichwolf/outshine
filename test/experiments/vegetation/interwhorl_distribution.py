"""Physical area at cached window centres; diagnostic, not measured image coverage."""

import argparse
import json
import math
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from spruce_growth_probe import cached_areas
from window_portrait_source import arrays


def area_weights(windows, recipe, areas):
    count = windows["needle_counts"].astype(np.uint64)
    phase = (
        windows["first_needle_ids"] % len(areas) + recipe["wood"]["seed"] % len(areas)
    ) % len(areas)
    values = (count // len(areas)).astype(float) * areas.sum()
    for variant, area in enumerate(areas):
        values += (
            (variant + len(areas) - phase) % len(areas) < count % len(areas)
        ) * area
    scale2 = np.array(
        [
            1 + p["scale_variation"] ** 2 / 3 if p else 1
            for p in recipe["physical_source"]["patterns"]
        ]
    )
    return values * scale2[windows["orders"]]


def distribution(product, areas):
    recipe = json.loads((product / "plant-recipe.json").read_text())
    report = json.loads((product / "execution/execution.json").read_text())
    windows = arrays(product / "windows.npz")
    weights = area_weights(windows, recipe, areas)
    np.testing.assert_allclose(weights.sum(), report["expected_area_m2"], rtol=2e-14)
    origins = windows["origins"]
    radius = np.linalg.norm(origins[:, :2], axis=1)
    heights = np.arange(0, 34.5, 0.5)
    radii = np.arange(0, 6.05, 0.1)
    h = grouped_area(origins[:, 2], heights, weights)
    r = grouped_area(radius, radii, weights)
    np.testing.assert_allclose(h.sum(), weights.sum(), rtol=2e-14)
    np.testing.assert_allclose(r.sum(), weights.sum(), rtol=2e-14)
    return {
        "identity": json.loads((product / "plant-product.json").read_text())[
            "plan_identity"
        ],
        "area_m2": float(weights.sum()),
        "height_edges_m": heights.tolist(),
        "height_area_m2": h.tolist(),
        "radius_edges_m": radii.tolist(),
        "radius_area_m2": r.tolist(),
        "area_at_radius_gt_2m": float(weights[radius > 2].sum()),
        "window_origin_bounds_m": [origins.min(0).tolist(), origins.max(0).tolist()],
        "scope": "Expected half closed needle surface at window centres. Not observed LAI, projected coverage, exact intra-window position or trunk-relative radius.",
    }


def grouped_area(coordinates, edges, weights):
    indices = np.searchsorted(edges, coordinates, side="right") - 1
    if np.any((indices < 0) | (indices >= len(edges) - 1)):
        raise ValueError("all physical area must remain inside the declared plot bins")
    # Accurate sums avoid subtracting nearly equal accumulated histogram totals.
    return np.array([math.fsum(weights[indices == i]) for i in range(len(edges) - 1)])


def compare(control, candidate, models, output):
    output.mkdir(parents=True, exist_ok=False)
    areas, hashes = cached_areas(models)
    rows = [distribution(p, areas) for p in (control, candidate)]
    fig, axes = plt.subplots(1, 2, figsize=(16, 9), dpi=160)
    for row, label in zip(
        rows,
        ("240 whorled primaries", "192 whorled +47 smaller primaries"),
        strict=True,
    ):
        h = np.array(row["height_edges_m"])
        axes[0].plot(row["height_area_m2"], (h[1:] + h[:-1]) / 2, label=label)
        axes[1].plot(
            row["radius_edges_m"][1:],
            np.cumsum(row["radius_area_m2"]) / row["area_m2"],
            label=label,
        )
    axes[0].set(
        xlabel="Expected needle area per0.5m height band [m²]",
        ylabel="Window-centre world height [m]",
    )
    axes[1].set(
        xlabel="Distance from world origin in XY [m]",
        ylabel="Cumulative expected needle-area fraction",
        ylim=(0, 1),
    )
    for ax in axes:
        ax.grid(alpha=0.25)
        ax.legend(loc="best", fontsize=9)
    fig.suptitle(
        "Picea abies | same physical area budget, different primary populations",
        fontsize=16,
    )
    fig.text(
        0.5,
        0.03,
        "Area assigned to cached window centres; not a coverage/LAI measurement. Bent trunk is not subtracted from XY radius.",
        ha="center",
        fontsize=10,
    )
    fig.tight_layout(rect=(0.02, 0.06, 0.98, 0.94))
    fig.savefig(output / "distribution.png")
    plt.close(fig)
    (output / "distribution.json").write_text(
        json.dumps({"cases": rows, "physical_input_sha256": hashes}, indent=2) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("control", "candidate", "models", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    compare(args.control, args.candidate, args.models, args.output)
