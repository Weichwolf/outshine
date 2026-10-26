"""Render the retained negative normal-group comparisons and native costs as PDF."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
from relief_distribution import CAPACITIES, MAX_PEAK_ERROR, RMS_PEAK_ERROR
from relief_shading import ANGLES


def summary(pdf, folders):
    fig, axes = plt.subplots(1, 3, figsize=(13, 4), constrained_layout=True)
    for name, folder in folders.items():
        report = json.loads((folder / "report.json").read_text())
        for smoothed in (False, True):
            maxima, rms, failed = [], [], []
            for cap in CAPACITIES:
                methods = [
                    next(m for m in c["methods"] if m["capacity"] == cap)
                    for c in report["cases"]
                ]
                if smoothed:
                    methods = [m["smoothed"] for m in methods]
                maxima.append(max(m["max_peak_error"] for m in methods))
                rms.append(max(m["rms_peak_error"] for m in methods))
                failed.append(sum(len(m["failed_curves"]) for m in methods))
            label = name + (" / local Toksvig" if smoothed else " / point groups")
            for ax, values in zip(axes, (maxima, rms, failed), strict=True):
                ax.plot(CAPACITIES, values, marker=".", label=label)
    for ax, title in zip(
        axes,
        ("Worst error / curve peak", "Worst RMS / curve peak", "Failed curves of288"),
        strict=True,
    ):
        ax.set_title(title)
        ax.set_xlabel("Maximum group capacity")
    axes[0].set_yscale("log")
    axes[1].set_yscale("log")
    axes[0].axhline(MAX_PEAK_ERROR, color="black", linestyle=":")
    axes[1].axhline(RMS_PEAK_ERROR, color="black", linestyle=":")
    axes[2].legend(fontsize=7)
    fig.suptitle("No tested capacity meets both fixed limits on every original curve")
    pdf.savefig(fig)
    plt.close(fig)


def counterexamples(pdf, folder):
    fig, axes = plt.subplots(1, 3, figsize=(13, 4), constrained_layout=True)
    for ax, name, row in zip(
        axes,
        ("joint-0.08", "sheared-corner-0.003", "joint-0.25"),
        (3, 11, 3),
        strict=True,
    ):
        with np.load(folder / f"{name}-9.npz") as raw:
            ax.plot(
                ANGLES,
                raw["reference"][row],
                label="independent reference",
                color="black",
            )
            ax.plot(ANGLES, raw["response"][row], label="9 point groups")
        with np.load(folder / f"{name}-129.npz") as raw:
            ax.plot(ANGLES, raw["response"][row], label="129 point groups")
            ax.plot(ANGLES, raw["smoothed_response"][row], label="129 / local Toksvig")
        ax.set_title(name + (" / r0.9" if row == 11 else " / r0.2"))
        ax.set_xlabel("Light angle, azimuth90deg; view50deg")
        ax.set_ylabel("Linear specular / unit irradiance")
    axes[0].legend(fontsize=7)
    fig.suptitle(
        "Retained angular-allocation failures; weak glossy curves remain part of the decision"
    )
    pdf.savefig(fig)
    plt.close(fig)


def costs(pdf, native):
    report = json.loads((native / "native.json").read_text())
    labels = [m["method"] for m in report["native"]]
    times = {
        name: [
            r["gpu_dispatch_sum_ms"]
            for r in report["gpu_timings"]
            if r["method"] == name
        ]
        for name in labels
    }
    x = np.arange(len(labels))
    fig, ax = plt.subplots(figsize=(10, 4), constrained_layout=True)
    for run, offset in ((0, -0.18), (1, 0.18)):
        ax.bar(
            x + offset,
            [times[n][run] for n in labels],
            width=0.36,
            label=("forward", "reverse")[run],
        )
    ax.set_xticks(x, labels)
    ax.set_ylabel("Actual GPU sum (ms),32 dispatches of115200 probes")
    ax.set_xlabel("Point-normal model capacity; ideal statistics already resident")
    ax.legend()
    fig.suptitle(
        "Native GGX work only; no statistics construction, light-loop or frame claim"
    )
    pdf.savefig(fig)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--area", type=Path, required=True)
    parser.add_argument("--angular", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with PdfPages(args.output) as pdf:
        summary(pdf, {"area": args.area, "angular": args.angular})
        counterexamples(pdf, args.angular)
        costs(pdf, args.native)


if __name__ == "__main__":
    main()
