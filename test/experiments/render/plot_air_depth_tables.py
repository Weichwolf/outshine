"""Plot the worst bounded-depth counterexample; requires Matplotlib, no runtime image."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output, root = args.output.resolve(), Path(__file__).resolve().parents[3]
    if output == root or root in output.parents:
        raise ValueError("private depth figures belong outside the repository")
    report = json.loads(args.report.read_text())
    if not report["reference_meets_targets"]:
        raise ValueError("reference convergence failed; no figure published")
    worst = max(
        report["results"]["quadratic_32_fp16"],
        key=lambda row: row["radiance_relative_max"],
    )
    figure, axes = plt.subplots(
        2, 1, figsize=(13, 7), sharex=True, layout="constrained"
    )
    for count in (32, 64, 128):
        rows = sorted(
            (
                row
                for row in report["results"][f"quadratic_{count}_fp16"]
                if row["case"] == worst["case"]
            ),
            key=lambda row: row["distance_km"],
        )
        distance = [row["distance_km"] for row in rows]
        axes[0].semilogy(
            distance,
            [max(1e-6, row["radiance_relative_max"] * 100) for row in rows],
            label=f"{count} quadratic depth nodes, FP16",
        )
        axes[1].plot(distance, [row["transmittance_absolute_max"] for row in rows])
    for axis in axes:
        axis.grid(True, alpha=0.25)
        axis.axvline(worst["distance_km"], color="#666666", linewidth=1, linestyle=":")
        axis.set_xlim(
            max(0.0, worst["distance_km"] * 0.8), min(200.0, worst["distance_km"] * 1.2)
        )
    axes[0].axhline(1.0, color="#9a456a", linestyle="--", label="1% experiment target")
    axes[0].set(ylabel="maximum RGB radiance error [%]", ylim=(1e-4, 250))
    axes[0].legend(fontsize=9)
    axes[1].axhline(0.002, color="#9a456a", linestyle="--")
    axes[1].set(
        xlabel="finite view segment [km]",
        ylabel="maximum absolute RGB T error",
        ylim=(0, 0.0022),
    )
    figure.suptitle(
        f"Depth interpolation counterexample: {worst['case']}\n"
        "Single Rayleigh scattering + ozone; radiance denominator floor 1e-5 sr⁻¹; no runtime proof",
        fontsize=12,
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output, dpi=180)
    plt.close(figure)
    print(output)


if __name__ == "__main__":
    main()
