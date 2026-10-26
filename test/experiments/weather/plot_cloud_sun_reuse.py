"""PDF of actual full-size transfer readbacks, work and the rejected profile."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from cloud_air_reference import SAMPLE
from matplotlib.backends.backend_pdf import PdfPages

ROOT = Path(__file__).resolve().parents[3]


def display(sample):
    rgb = np.clip(sample["radiance"] * 8 + sample["through"] * [0.02, 0.03, 0.04], 0, 1)
    return np.where(rgb <= 0.0031308, rgb * 12.92, 1.055 * rgb ** (1 / 2.4) - 0.055)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private output belongs outside the checkout")
    root = args.captures
    report = json.loads((root / "receipt.json").read_text())
    full = report["benchmark"]
    direct = np.fromfile(root / "full-1920x1080/direct.bin", SAMPLE)
    reused = np.fromfile(root / "full-1920x1080/reuse.bin", SAMPLE)
    shape = (full["height"], full["width"])
    error = np.max(
        abs(direct["radiance"] - reused["radiance"])
        / np.maximum(abs(direct["radiance"]), 1e-5),
        axis=1,
    ).reshape(shape)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with PdfPages(args.output) as pdf:
        figure, axes = plt.subplots(2, 2, figsize=(13, 8))
        for axis, sample, title in zip(
            axes[0],
            (direct, reused),
            ("Direct Sun-T6", "Local column maps"),
            strict=True,
        ):
            axis.imshow(display(sample).reshape(*shape, 3), interpolation="none")
            axis.set_title(title)
            axis.axis("off")
        image = axes[1, 0].imshow(
            error * 100, vmin=0, vmax=2, interpolation="none", cmap="magma"
        )
        axes[1, 0].set_title("Native RGB change versus direct (%)")
        figure.colorbar(image, ax=axes[1, 0], fraction=0.04)
        axes[1, 1].axis("off")
        witness = full["independent_witnesses"]
        summary = (
            "1920 × 1080 actual compute readbacks\n"
            "Same prescribed clear/Psi inputs, density and display exposure\n\n"
            f"Maximum native change: {error.max() * 100:.4f}%\n"
            f"p99 native change: {np.quantile(error, 0.99) * 100:.4f}%\n"
            f"Worst refined RTE witness, reused: {max(x['reuse_relative'] for x in witness) * 100:.4f}%\n"
            f"Worst refined RTE witness, direct: {max(x['direct_relative'] for x in witness) * 100:.4f}%\n\n"
            "Unchanged 1% RGB / 1e-5 floor\n"
            "The map path is rejected as this runtime profile."
        )
        axes[1, 1].text(0.02, 0.9, summary, va="top", fontsize=10)
        figure.suptitle("Solar-column reuse: full-size numerical image", fontsize=15)
        figure.text(
            0.5,
            0.015,
            "Displayed L/T adds the same constant background; no engine/Place, physical LUT or diffuse-cloud convergence claim.",
            ha="center",
            fontsize=9,
        )
        figure.tight_layout(rect=(0, 0.04, 1, 0.94))
        figure.subplots_adjust(hspace=0.3)
        pdf.savefig(figure)
        plt.close(figure)

        figure, axes = plt.subplots(1, 2, figsize=(12, 5))
        profiles = full["profiles"]
        axes[0].bar(
            [p["profile"] for p in profiles], [p["warm_median_ms"] for p in profiles]
        )
        axes[0].axhline(
            1000 / 60, color="red", ls=":", label="16.67 ms whole-frame target"
        )
        axes[0].set_ylabel("Actual Dispatch timestamp interval (ms)")
        axes[0].legend(fontsize=8)
        work = {p["profile"]: p for p in report["cases"]["0.65"]["profiles"]}
        axes[1].bar(
            ["direct", "reuse"],
            [work[n]["sum_air_solar_density"] for n in ("direct-work", "reuse-work")],
        )
        axes[1].set_ylabel("Actual additional solar density evaluations")
        axes[1].set_title("30-ray cohort; each ray counted once")
        figure.suptitle(
            "Source reuse reduces work; the per-pixel integrator still fails this profile"
        )
        figure.text(
            0.5,
            0.025,
            "Dispatch values include their observed contention case; load snapshots are in the receipt.\n"
            "Map construction is CPU/upload here; GPU generation and complete frame costs remain unproven.\n"
            "Three R32F levels: 3,158,028 bytes / virtual camera; no selected production profile.",
            ha="center",
            fontsize=9,
        )
        figure.tight_layout(rect=(0, 0.17, 1, 0.9))
        pdf.savefig(figure)
        plt.close(figure)


if __name__ == "__main__":
    main()
