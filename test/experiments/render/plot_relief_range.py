"""Private diagnostics for the finite residual-range comparison (#948)."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from relief_shading import ANGLES


def plot(output):
    report = json.loads((output / "report.json").read_text())
    with np.load(output / "comparison.npz") as data:
        fig, axes = plt.subplots(2, 2, figsize=(14, 10), layout="constrained")
        for ax, curve, label in (
            (axes[0, 0], 3, "r=0.2 / view50° / azimuth90°"),
            (axes[0, 1], 11, "r=0.9 / view50° / azimuth90°"),
        ):
            ax.fill_between(
                ANGLES,
                data["intervals"][curve, :, 0],
                data["intervals"][curve, :, 1],
                color="orange",
                alpha=0.25,
                label="model range; rounding not certified",
            )
            ax.plot(
                ANGLES,
                data["reference"][curve],
                color="black",
                label="independent reference",
            )
            ax.plot(ANGLES, data["baseline"][curve], "--", label="original centroid")
            ax.plot(ANGLES, data["response"][curve], label="adaptive range midpoint")
            ax.set_title(label + " — unchanged sheared-corner-0.003")
            ax.set_xlabel("light angle / degrees")
            ax.set_ylabel("linear direct specular response")
            ax.legend(fontsize=8)
        ax = axes[1, 0]
        ids = np.arange(12)
        ax.bar(
            ids - 0.18,
            np.asarray(report["baseline"]["rms_peak_error"]) * 100,
            width=0.36,
            label="original centroid",
        )
        ax.bar(
            ids + 0.18,
            np.asarray(report["adaptive"]["rms_peak_error"]) * 100,
            width=0.36,
            label="adaptive, <=64 leaves",
        )
        ax.axhline(0.5, color="red", linestyle="--", label="unchanged 0.5% RMS limit")
        ax.set_xlabel("original curve index; 0–3 glossy, 8–11 matte")
        ax.set_ylabel("RMS / own reference peak / %")
        ax.set_yscale("log")
        ax.set_xticks(ids)
        ax.legend(fontsize=8)
        history = data["history"]
        ax = axes[1, 1]
        ax.fill_between(
            history[:, 0],
            history[:, 2],
            history[:, 3],
            color="orange",
            alpha=0.3,
            label="positive model range",
        )
        ax.plot(history[:, 0], history[:, 1], label="midpoint")
        index = int(np.flatnonzero(ANGLES == -35)[0])
        value = data["reference"][3, index]
        ax.axhline(
            value,
            color="black",
            label="independent reference; absent from stopping criterion",
        )
        ax.set_yscale("log")
        ax.set_xlabel("active leaves, one adaptive run")
        ax.set_ylabel("linear response; glossy/view50/light−35/azimuth90")
        ax.set_title("Priority follows largest weighted residual uncertainty")
        ax.legend(fontsize=8)
        for ax in axes.flat:
            ax.grid(True, alpha=0.2)
        fig.savefig(output / "range-comparison.png", dpi=120)
        plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    plot(parser.parse_args().output)
