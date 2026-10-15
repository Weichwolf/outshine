"""Inspect actual GPU Weather/air transfers and separate source work from quality."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from cloud_air_reference import RAY, SAMPLE
from matplotlib.backends.backend_pdf import PdfPages

ROOT = Path(__file__).resolve().parents[3]


def display(rgb):
    value = np.clip(rgb, 0, 1)
    return np.where(
        value <= 0.0031308, 12.92 * value, 1.055 * value ** (1 / 2.4) - 0.055
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private outputs belong outside the checkout")
    root, case = args.captures, args.captures / "cover-0.65"
    receipt = json.loads((root / "receipt.json").read_text())
    rays = np.fromfile(root / "rays.bin", RAY)
    queries = np.fromfile(root / "queries.bin", "<f4")
    reference = np.load(case / "reference.npy")
    names = ("quadratic22", "log256", "unshadowed", "whole-source", "bad-origin")
    records = {name: np.fromfile(case / name / "native.bin", SAMPLE) for name in names}
    error = np.max(
        np.abs(records["unshadowed"]["radiance"] - reference[:, :3])
        / np.maximum(np.abs(reference[:, :3]), 1e-5),
        axis=1,
    )
    # Select the strongest source counterexample from the actual native readback.
    ray = rays[
        next(i for i, r in enumerate(rays) if r["begin"] <= error.argmax() < r["end"])
    ]
    rows = slice(int(ray["begin"]), int(ray["end"]), 2)
    distance, expected = queries[rows], reference[rows]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with PdfPages(args.output) as pdf:
        figure, axes = plt.subplots(1, 3, figsize=(13, 5))
        axes[0].plot(
            distance, expected[:, 1] * 1000, color="black", label="Independent FP64"
        )
        strips = [display(expected[:, :3] * 8 + expected[:, 3:] * 0.02)]
        for name in names:
            capture = records[name][rows]
            axes[0].plot(distance, capture["radiance"][:, 1] * 1000, label=name)
            delta = np.max(
                np.abs(capture["radiance"] - expected[:, :3])
                / np.maximum(np.abs(expected[:, :3]), 1e-5),
                axis=1,
            )
            axes[1].plot(distance, delta * 100, label=name)
            strips.append(display(capture["radiance"] * 8 + capture["through"] * 0.02))
        axes[0].set_ylabel("Green radiance / lux (10⁻³ sr⁻¹)")
        axes[1].set_ylabel("Maximum RGB error (%)")
        axes[1].axhline(1, color="grey", ls=":")
        for axis in axes[:2]:
            axis.set_xlabel("World query distance (km)")
            axis.grid(alpha=0.2)
            axis.legend(fontsize=7)
        axes[2].imshow(
            np.asarray(strips),
            aspect="auto",
            interpolation="nearest",
            extent=(distance[0], distance[-1], len(strips) - 0.5, -0.5),
        )
        axes[2].set_yticks(range(len(strips)), ["FP64", *names])
        axes[2].set_xlabel("World query distance (km)")
        axes[2].set_title("Captured L/T, one shared display exposure", fontsize=9)
        figure.suptitle("Weather solar transfer at the actual air-source point")
        figure.text(
            0.5,
            0.02,
            f"Eye radius {ray['ray'][0]:.6f} km; view cosine {ray['ray'][1]:.3f}; cover0.65.\n"
            "Actual GPU transfer plus constant background; prescribed clear/Psi inputs. No Place/frame-budget claim.",
            ha="center",
            fontsize=8,
        )
        figure.tight_layout(rect=(0, 0.1, 1, 0.93))
        pdf.savefig(figure)
        plt.close(figure)
        labels, errors, work = [], [], []
        for name in ("quadratic22", "log64", "log128", "log256"):
            profiles = [
                next(p for p in c["profiles"] if p["profile"] == name)
                for c in receipt["cases"].values()
            ]
            labels.append(name)
            errors.append(100 * max(p["max_relative"] for p in profiles))
            work.append(max(p["max_air_sources"] for p in profiles))
        figure, axes = plt.subplots(1, 2, figsize=(11, 5))
        axes[0].bar(labels, errors)
        axes[0].set_yscale("log")
        axes[0].axhline(1, color="grey", ls=":")
        axes[0].set_ylabel("Worst native relative RGB error (%)")
        axes[1].bar(labels, work)
        axes[1].set_ylabel("Peak logical air-source positions / ray")
        figure.suptitle("Actual Weather cohort: quality and source work")
        figure.text(
            0.5,
            0.025,
            "26 rays × 81 duplicated queries × 3 covers; sparse/null records remain bitwise identical.\n"
            "Work is algorithmic source positions, not GPU time or instructions. 256 cells remain experimental.",
            ha="center",
            fontsize=9,
        )
        figure.tight_layout(rect=(0, 0.13, 1, 0.93))
        pdf.savefig(figure)
        plt.close(figure)


if __name__ == "__main__":
    main()
