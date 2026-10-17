"""Plot actual old/new long-phase sky captures, independent errors and native costs."""

import argparse
import json
import tempfile
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages


def image(folder, profile, rate):
    stem = folder / f"actual.wind-{profile}-1-{rate}"
    metadata = json.loads(Path(str(stem) + ".json").read_text())
    return np.fromfile(str(stem) + ".images.bin", "<f4").reshape(
        5, metadata["height"], metadata["width"], 4
    )[1, :, :, :3]


def motion_page(pdf, folder):
    rows = []
    for profile in (0, 1):
        old = image(folder / "0-legacy", profile, 0)
        new = image(folder / "1-shared", profile, 0)
        rows.append(
            (
                old,
                image(folder / "0-legacy", profile, 60) - old,
                new,
                image(folder / "1-shared", profile, 60) - new,
            )
        )
    limit = max(
        0.01, max(float(np.max(abs(row[3].mean(axis=2)))) * 10000 for row in rows)
    )
    fig, axes = plt.subplots(2, 4, figsize=(14, 7.4))
    for profile, row in enumerate(rows):
        for column, rgb in enumerate(row):
            ax = axes[profile, column]
            if column in (1, 3):
                ax.imshow(
                    rgb.mean(axis=2) * 10000, cmap="RdBu_r", vmin=-limit, vmax=limit
                )
            else:
                ax.imshow(4 * np.maximum(rgb, 0) / (1 + 4 * np.maximum(rgb, 0)))
            ax.set_xticks([])
            ax.set_yticks([])
            if column == 0:
                ax.set_ylabel(("Stratus", "Cumulus V1")[profile])
            ax.set_title(
                (
                    "legacy at32days",
                    "legacy +1/60s delta x10000",
                    "shared at32days",
                    "shared +1/60s delta x10000",
                )[column],
                fontsize=10,
            )
    height, width = rows[0][0].shape[:2]
    fig.suptitle(
        "Actual native sky after32days: old RGB sticks; C++/shared-layer wind advances"
    )
    fig.text(
        0.07,
        0.035,
        f"{width}x{height} actual raster; same physical rays, air, light, cloud cover0.65. RGB display4x/(1+4x).\nDelta is mean raw RGB, common colour range +/-{limit:.3g}. Private sky probe, not a world/Place image.",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0.09, 1, 0.95))
    pdf.savefig(fig)
    plt.close(fig)


def errors_page(pdf, reference):
    fig, axes = plt.subplots(1, 2, figsize=(11.7, 7.4))
    rows = reference["cases"]
    for key, label in (
        ("max_radiance_error", "radiance"),
        ("max_transmittance_error", "transmittance"),
        ("max_composition_error", "composed RGB"),
    ):
        axes[0].semilogy(
            range(len(rows)),
            [max(row[key], 1e-10) for row in rows],
            marker="o",
            label=label,
        )
    axes[0].axhline(
        1e-4, color="firebrick", linestyle="--", label="unchanged absolute limit"
    )
    axes[0].set_xlabel("all16 profile/phase/rate/anchor cases")
    axes[0].set_ylabel("max absolute error against independent FP64")
    axes[0].legend(fontsize=9)
    axes[0].grid(alpha=0.2)
    steps = reference["steps"]
    axes[1].bar(
        range(len(steps)),
        [100 * row["relative_transfer_step_l2_error"] for row in steps],
    )
    axes[1].set_xticks(
        range(len(steps)),
        [f"{row['case']}\n{row['rate_hz']}Hz" for row in steps],
        rotation=30,
        ha="right",
        fontsize=8,
    )
    axes[1].set_ylabel("actual sky step relative L2 error (%)")
    axes[1].grid(axis="y", alpha=0.2)
    pixels = sum(row["profile"][0] * row["profile"][1] for row in rows)
    fig.suptitle(
        f"All {pixels} captured pixels checked; requested phase remains an independent input"
    )
    fig.text(
        0.07,
        0.03,
        "Both signed32day phases,30/60Hz and non-FP32-exact field anchors. Physical rays and lighting bytes stay identical.\nSky step errors are measured; the separate unchanged point-density5% gate also passes all208 captures.\nNo new limits, reference regeneration, history or whole-device acceptance.",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0.12, 1, 0.94))
    pdf.savefig(fig)
    plt.close(fig)


def costs_page(pdf, costs):
    fig, axes = plt.subplots(
        1, 2, figsize=(11.7, 7.4), gridspec_kw={"width_ratios": [1, 1.5]}
    )
    data = [
        [
            value / 1e6
            for run in costs["runs"]
            if run["variant"] == label
            for case in run["cases"]
            for value in case["submission_gpu_ns"]
        ]
        for label in ("legacy", "shared")
    ]
    axes[0].boxplot(data, tick_labels=["legacy", "shared112B"], whis=(0, 100))
    axes[0].set_ylabel("actual GPU submission (ms)")
    axes[0].grid(axis="y", alpha=0.2)
    axes[1].axis("off")
    rows = costs["timing"]
    width, height = costs["profile"]
    pixels = width * height
    readback = 5 * ((width * 16 + 255) // 256 * 256) * height
    text = [
        f"A/B/B/A, fresh processes;{rows['legacy']['samples']} actual GPU samples/variant.",
        "Both draws retained, including first draw; validation off.",
        f"Median: {rows['legacy']['median_ns'] / 1e6:.6f} -> {rows['shared']['median_ns'] / 1e6:.6f} ms.",
        "Includes scene/sky and five readback copies, not CPU IO/fence time.",
        "Shared desktop; process snapshots retained. No FPS/device proof.",
        "",
        "Same physical rays, texture sizes and requested contents.",
        "Private weather packet96 ->160B;112B cloud core +48B eye/light.",
        "Medium80/View224 and composition unchanged.",
        f"Logical texture texels{84 * pixels + 64}B; five readbacks{readback}B/frame.",
        f"CPU capture vectors{80 * pixels}B/frame; backend allocation/peak separate.",
        "No additional pass or world traversal; retained/in-flight residency open.",
        "",
        "Old variant fails motion quality; this is no quality-parity speedup.",
        "53 existing boundary/profile cases retain their fixed FP64 limits.",
        "Dense legacy clear-air gate remains red; old/new outputs byte identical.",
        "Shared Check.h analysis findings and production/world/history/Metal open.",
    ]
    axes[1].text(0.02, 0.97, "\n\n".join(text), va="top", fontsize=9)
    fig.suptitle(
        "Separate measured component cost; no budget inferred from current consumption"
    )
    fig.tight_layout(rect=(0, 0.02, 1, 0.94))
    pdf.savefig(fig)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("comparison", type=Path)
    args = parser.parse_args()
    folder = args.comparison.resolve()
    if not folder.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private visual evidence belongs in system temp")
    reference = json.loads((folder / "1-shared/wind-reference.json").read_text())
    costs = json.loads((folder / "receipt.json").read_text())
    with PdfPages(folder / "native-wind-sky.pdf") as pdf:
        motion_page(pdf, folder)
        errors_page(pdf, reference)
        costs_page(pdf, costs)


if __name__ == "__main__":
    main()
