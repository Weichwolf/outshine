"""Plot resident GPU state transitions, reconstruction error and measured costs."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from cloud_air_reference import SAMPLE
from cloud_air_transfer_reuse import numeric
from matplotlib.backends.backend_pdf import PdfPages


def preview(values):
    linear = np.clip(4 * (values[..., :3] + values[..., 3:] * 0.02), 0, 1)
    return np.where(
        linear <= 0.0031308, linear * 12.92, 1.055 * linear ** (1 / 2.4) - 0.055
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private output belongs outside the checkout")
    receipt = json.loads((args.captures / "receipt.json").read_text())
    config = json.loads(
        (args.captures / f"step-{receipt['step']}/grid.json").read_text()
    )
    images = {
        name: [
            numeric(np.fromfile(args.captures / name / f"gpu-state-{i}.bin", SAMPLE))
            for i in range(3)
        ]
        for name in ("bilinear", "fused", "staged")
    }
    fields = [
        np.fromfile(args.captures / "fused" / f"gpu-state-{i}.field", "<f4").reshape(
            *config["shape"], 4
        )
        for i in range(3)
    ]
    labels = [
        "Covered,37s advection",
        "Clear, same resident resource",
        "Covered, zero advection",
    ]
    args.output.parent.mkdir(exist_ok=True, parents=True)
    with PdfPages(args.output) as pdf:
        figure, axes = plt.subplots(3, 2, figsize=(14, 13))
        for state, label in enumerate(labels):
            axes[state, 0].imshow(
                preview(images["fused"][state].reshape(1080, 1920, 6)),
                interpolation="nearest",
            )
            axes[state, 0].set_title(label + ": actual GPU air transfer")
            axes[state, 1].imshow(
                np.clip(fields[state][..., :3], 0, 1), interpolation="nearest"
            )
            axes[state, 1].set_title("Actual GPU weighted-visibility field, linear0–1")
        for axis in axes.ravel():
            axis.axis("off")
        figure.suptitle(
            "Resident field updates; no CPU roundtrip between dependent passes",
            fontsize=15,
        )
        figure.text(
            0.5,
            0.012,
            "1920×1080,10km, homogeneous-air control; cloud eye-extinction/emission excluded.\nAir previews share exposure4, background0.02 and display transform. Return-state bytes are checked separately.",
            ha="center",
            fontsize=9,
        )
        figure.tight_layout(rect=(0, 0.05, 1, 0.96))
        figure.subplots_adjust(hspace=0.3)
        pdf.savefig(figure)
        plt.close(figure)

        direct = numeric(np.fromfile(args.captures / "direct-stationary.bin", SAMPLE))
        figure, axes = plt.subplots(2, 3, figsize=(15, 9))
        for index, name in enumerate(("bilinear", "fused", "staged")):
            delta = (
                np.max(
                    abs(images[name][2][:, :3] - direct[:, :3])
                    / np.maximum(abs(direct[:, :3]), 1e-5),
                    axis=-1,
                )
                * 100
            )
            image = axes[0, index].imshow(
                delta.reshape(1080, 1920), vmin=0, vmax=1.5, cmap="magma"
            )
            axes[0, index].set_title(
                f"{name}: actual RGB change, max{delta.max():.3g}%"
            )
            axes[0, index].axis("off")
            figure.colorbar(
                image, ax=axes[0, index], shrink=0.65, label="Channel change (%)"
            )
            witnesses = receipt["analysis"]["profiles"][name]["states"][2]["witnesses"]
            axes[1, index].scatter(
                range(len(witnesses)),
                [r["reused_RGB_relative"] * 100 for r in witnesses],
                s=20,
            )
            axes[1, index].axhline(1, color="gray", ls=":")
            axes[1, index].set_ylim(0, 1.5)
            axes[1, index].set_ylabel("Independent Gauss16/32 RGB error (%)")
            axes[1, index].set_xlabel("Worst/fixed image witnesses")
            axes[1, index].grid(alpha=0.2)
        figure.suptitle(
            "Added wind state rejects bilinear; same field with cubic reconstruction"
        )
        figure.text(
            0.5,
            0.015,
            "All pixels compare with the actual direct-source image; finite independent witnesses use unchanged1% RGB.\nEvery reconstructed T additionally compares with the independent analytic result at unchanged2e−6.",
            ha="center",
            fontsize=9,
        )
        figure.tight_layout(rect=(0, 0.07, 1, 0.95))
        pdf.savefig(figure)
        plt.close(figure)

        figure, axes = plt.subplots(1, 2, figsize=(13, 5))
        states = np.arange(4)
        for shift, name in enumerate(("bilinear", "fused", "staged")):
            rows = receipt["profiles"][name]["rows"]
            medians = [
                np.median(
                    [
                        r["pair_span_ms"]
                        for r in rows
                        if r["state"] == state and r["repeat"] > 0
                    ]
                )
                for state in states
            ]
            axes[0].bar(states + (shift - 1) * 0.25, medians, width=0.25, label=name)
        axes[0].set_xticks(states, ["Covered37s", "Clear", "Covered0s", "Return37s"])
        axes[0].set_ylabel("Actual dependent GPU span, warm median (ms)")
        axes[0].legend(fontsize=8)
        axes[0].grid(axis="y", alpha=0.2)
        for name in ("bilinear", "fused", "staged"):
            states_data = receipt["analysis"]["profiles"][name]["states"]
            axes[1].plot(
                range(3),
                [r["T_analytic_abs_max"] for r in states_data],
                "o-",
                label=name,
            )
        axes[1].axhline(2e-6, color="gray", ls=":", label="Unchanged T bound")
        axes[1].set_xticks(range(3), ["Covered37s", "Clear", "Covered0s"])
        axes[1].set_ylabel("Worst all-pixel T error against closed form")
        axes[1].legend(fontsize=8)
        axes[1].grid(alpha=0.2)
        figure.suptitle(
            "Measured state-dependent cost and optical depth; not a whole-frame budget"
        )
        figure.text(
            0.5,
            0.015,
            "Span includes actual GPU transition gaps. Initial upload, diagnostic readback/IO and other renderer stages are separate.\nNo arbitrary-depth, joint-cloud, history, Place, p99 or A18 acceptance.",
            ha="center",
            fontsize=9,
        )
        figure.tight_layout(rect=(0, 0.12, 1, 0.94))
        pdf.savefig(figure)
        plt.close(figure)


if __name__ == "__main__":
    main()
