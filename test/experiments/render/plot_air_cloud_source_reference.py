"""Plot the independent source-shadow counterexamples as a local PDF (#320)."""

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from air_cloud_source_reference import MODES, ROOT, Slab, analytic
from matplotlib.backends.backend_pdf import PdfPages

LABELS = (
    "Direct source shadowed at each point",
    "Direct source left unshadowed",
    "Entire source dimmed at each point",
    "Entire eye radiance dimmed afterwards",
)
COLORS = ("#164d82", "#db5829", "#8a478d", "#59933f")


def plot_case(axes, slab, title):
    distance = np.linspace(0, slab.reach(12), 241)
    correct = np.array([analytic(slab, query) for query in distance])
    for mode, label, color in zip(MODES, LABELS, COLORS, strict=True):
        result = np.array([analytic(slab, query, mode) for query in distance])
        axes[0].plot(distance, result[:, 1] * 1e3, label=label, color=color)
        error = np.max(np.abs(result - correct) / np.maximum(1e-5, correct), axis=1)
        axes[1].plot(distance, error * 100, color=color)
    axes[0].set_title(title, loc="left", fontsize=12)
    axes[0].set_ylabel("Green radiance / lux (10⁻³ sr⁻¹)")
    axes[1].set_ylabel("Maximum RGB error (%)")
    for axis in axes:
        axis.set_xlabel("Camera query distance (km)")
        axis.grid(alpha=0.2)
        axis.set_xlim(distance[0], distance[-1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private output belongs outside the checkout")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    cases = (
        (Slab(), "Eye below the cloud; camera exits through the cloud"),
        (
            Slab(eye_height=1.5, view_mu=0.6, sun_mu=0.4),
            "Eye inside the cloud; oblique sun and view",
        ),
        (
            Slab(eye_height=3, view_mu=-0.3),
            "Eye above the cloud; camera crosses the cloud downwards",
        ),
        (
            Slab(view_mu=0, sun_mu=0.4),
            "Cloud never intersects the horizontal camera ray",
        ),
    )
    with PdfPages(args.output) as pdf:
        for begin in (0, 2):
            figure, axes = plt.subplots(2, 2, figsize=(12, 8.5))
            for row, (slab, title) in enumerate(cases[begin : begin + 2]):
                plot_case(axes[row], slab, title)
            figure.suptitle(
                "Cloud shadows belong in the direct air source", fontsize=17
            )
            handles, labels = axes[0, 0].get_legend_handles_labels()
            figure.legend(
                handles,
                labels,
                loc="lower center",
                bbox_to_anchor=(0.5, 0.065),
                ncol=2,
                fontsize=10,
            )
            figure.text(
                0.5,
                0.018,
                "Independent exact piecewise exponential transport; prescribed diffuse input held fixed.\n"
                "Pure absorbing cloud: 1–2 km, extinction 1.5 km⁻¹. No runtime or diffuse-cloud closure claim.",
                ha="center",
                fontsize=10,
            )
            figure.tight_layout(rect=(0, 0.17, 1, 0.94))
            pdf.savefig(figure)
            plt.close(figure)


if __name__ == "__main__":
    main()
