"""Compare the bounded air prefix to rejected depth interpolation and direct quadrature."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def curve(axis, rows, label, **style):
    rows = sorted(rows, key=lambda row: row["distance_km"])
    axis.plot(
        [row["distance_km"] for row in rows],
        [max(1e-8, row["radiance_relative_max"] * 100.0) for row in rows],
        label=label,
        **style,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", type=Path)
    parser.add_argument("depth", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output, root = args.output.resolve(), Path(__file__).resolve().parents[3]
    if output == root or root in output.parents:
        raise ValueError("private air-prefix figures belong outside the repository")
    prefix, depth = (
        json.loads(args.prefix.read_text()),
        json.loads(args.depth.read_text()),
    )
    if not prefix["reference_meets_targets"] or not depth["reference_meets_targets"]:
        raise ValueError("reference convergence failed; no figure published")
    if prefix["inputs_sha256"] != depth["inputs_sha256"]:
        raise ValueError("the compared inputs differ")
    old_worst = max(
        depth["results"]["quadratic_32_fp16"],
        key=lambda row: row["radiance_relative_max"],
    )
    new_worst = max(
        prefix["results"]["quadratic_19"], key=lambda row: row["radiance_relative_max"]
    )
    figure, axes = plt.subplots(1, 2, figsize=(15, 6), layout="constrained")
    for count in (32, 128):
        rows = [
            row
            for row in depth["results"][f"quadratic_{count}_fp16"]
            if row["case"] == old_worst["case"]
        ]
        curve(axes[0], rows, f"{count}-node regular depth table, FP16", linestyle="--")
    rows = [
        row
        for row in prefix["results"]["quadratic_19"]
        if row["case"] == old_worst["case"]
    ]
    curve(axes[0], rows, "19-cell coefficient prefix", color="#216e47")
    axes[0].set(title=old_worst["case"], xlim=(140.0, 200.0), ylim=(1e-4, 250.0))
    for count in (4, 8):
        rows = [
            dict(
                row["direct_comparison"][f"gauss_{count}"],
                distance_km=row["distance_km"],
            )
            for row in prefix["references"]
            if row["case"] == new_worst["case"]
        ]
        curve(axes[1], rows, f"direct split Gauss-{count} per query", linestyle="--")
    rows = [
        row
        for row in prefix["results"]["quadratic_19"]
        if row["case"] == new_worst["case"]
    ]
    curve(axes[1], rows, "19-cell coefficient prefix", color="#216e47")
    axes[1].set(
        title=new_worst["case"], xscale="log", xlim=(0.001, 200.0), ylim=(1e-5, 3.0)
    )
    for axis in axes:
        axis.set(
            yscale="log",
            xlabel="finite view segment [km]",
            ylabel="maximum RGB radiance error [%]",
        )
        axis.axhline(1.0, color="#9a456a", linestyle=":", label="1% experiment target")
        axis.grid(True, alpha=0.25)
        axis.legend(fontsize=8)
    figure.suptitle(
        "Clear-gas model: independently refined depths, denominator floor 1e-5 sr⁻¹\nPrefix reuses sources across monotone queries; direct quadrature repeats them. No runtime/GPU-cost proof.",
        fontsize=12,
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output, dpi=180)
    plt.close(figure)
    print(output)


if __name__ == "__main__":
    main()
