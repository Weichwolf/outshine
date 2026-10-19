"""Local scientific diagnostics; no runtime-world image or native performance claim."""

import argparse
import json
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from far_product_cut import projection_diameter


def plot(report, output):
    fig, axes = plt.subplots(1, 2, figsize=(12.8, 7.2), dpi=100, layout="constrained")
    p = np.array([640.0, 0, 1000])
    q = p + [0.99, 0, -0.6336]
    box = np.array([np.minimum(p, q), np.maximum(p, q)])
    naive = 1000 * np.linalg.norm(q - p) / np.linalg.norm(box[0])
    actual = 1000 * (q[0] / q[2] - p[0] / p[2])
    safe = projection_diameter(
        box, np.zeros((2, 3)), focal_px=1000, width=1280, height=720
    )
    ax = axes[0]
    ax.axvspan(-1, 1, color="#c9e4cf", label="1 px geometric budget")
    for value, label, color in (
        (0, "source point", "#24624e"),
        (naive, f"Euclidean estimate {naive:.3f} px", "#997b28"),
        (actual, f"actual displacement {actual:.3f} px", "#b62e3c"),
        (safe, f"new upper bound {safe:.3f} px", "#3267a6"),
    ):
        ax.axvline(value, color=color, label=label)
    ax.set(
        xlim=(-0.3, safe + 0.25),
        ylim=(0, 1),
        yticks=[],
        xlabel="Horizontal screen displacement (pixels)",
        title="Off-axis counterexample at the viewport edge",
    )
    ax.legend(loc="upper left", fontsize=9)
    ax.text(
        0.03,
        0.1,
        "1280×720, focal 1000 px\nPoint-set displacement only; not opacity.",
        transform=ax.transAxes,
        fontsize=10,
    )
    ax = axes[1]
    for row, color in zip(
        report["results"], ("#24624e", "#3267a6", "#997b28"), strict=True
    ):
        x = [c["eye_altitude_offset_m"] / 1000 for c in row["cases"]]
        for method, style in (("shared", "-"), ("cells", "--")):
            y = [
                100 * c[method]["work"]["proxy_sources"] / row["sources"]
                for c in row["cases"]
            ]
            ax.plot(
                x,
                y,
                style,
                marker="o",
                color=color,
                label=f"{row['sources']:,} source records / {method}",
            )
    ax.set(
        xlabel="Declared eye-height offset (km)",
        ylabel="Source records in box candidates (%)",
        ylim=(0, 100),
        title="Complete source cut; same 1 px rule, all directions",
    )
    ax.legend(fontsize=8)
    ax.grid(alpha=0.2)
    fig.suptitle("Conservative far-box baseline: no ground-view ROI", fontsize=15)
    fig.savefig(output)
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if target == temporary or not target.is_relative_to(temporary) or target.exists():
        parser.error("fresh explicit system-temp image required")
    plot(json.loads(args.report.read_text()), target)
