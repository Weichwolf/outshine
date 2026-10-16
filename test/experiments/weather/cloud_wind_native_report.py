"""Render native wind evidence as a PDF; no scene-render or device-budget claim."""

import argparse
import json
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages


def fields(pdf, record):
    fig, axes = plt.subplots(1, 2, figsize=(11.7, 6.5))
    for profile, ax in enumerate(axes):
        rows = [row for row in record["field_cases"] if row["profile"] == profile]
        phases = list(dict.fromkeys(row["phase_s"] for row in rows))
        for cover in (0.0, 0.45, 0.65, 1.0):
            selected = [row for row in rows if row["cover"] == cover]
            ax.semilogy(
                range(len(phases)),
                [max(row["max_normalized_density_error"], 1e-9) for row in selected],
                marker="o",
                label=f"cover {cover}",
            )
        ax.axhline(1e-4, color="firebrick", linestyle="--", label="unchanged limit")
        ax.set_xticks(
            range(len(phases)), ["0", "37 s", "+32 d", "-32 d", "10 y", "1.79e9 s"]
        )
        ax.set_title(("Stratus", "Cumulus V1")[profile])
        ax.set_ylim(1e-9, 3e-4)
        ax.set_ylabel("max |native - independent FP64| / extinction")
        ax.grid(alpha=0.2)
        ax.legend()
    maximum = max(row["relative_step_l2_error"] for row in record["temporal_cases"])
    fig.suptitle(
        "Native split-lattice field: actual GPU output including spherical height"
    )
    fig.text(
        0.08,
        0.035,
        f"48 field cases, 48 long-phase wind steps at30/60Hz; worst step L2 relative error {maximum:.4%}.\n"
        "24 cell-boundary steps,12 anchors; eight original defaults byte identical. Private112B payload; no production ABI or device acceptance.",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0.11, 1, 0.94))
    pdf.savefig(fig)
    plt.close(fig)


def motion(pdf, record, folder):
    plane = record["plane"]
    fig, axes = plt.subplots(2, 3, figsize=(11.7, 7.4))
    for row_index, split in enumerate((False, True)):
        rows = [row for row in plane["rows"] if row["split"] == split]
        values = [
            np.fromfile(folder / f"{row['name']}.density.bin", "<f4").reshape(
                plane["height"], plane["width"]
            )
            / 0.012
            for row in rows
        ]
        for col, index in enumerate((0, 1, 3)):
            ax = axes[row_index, col]
            if index == 1:
                image = (values[index] - values[0]) * 10000
                ax.imshow(
                    image,
                    origin="lower",
                    extent=(-2800, 2800, -1680, 1680),
                    cmap="RdBu_r",
                    vmin=-0.5,
                    vmax=0.5,
                )
                ax.set_title(
                    f"+1/60 s difference x10000\n{rows[index]['changed_points']} changed samples"
                )
            else:
                ax.imshow(
                    values[index],
                    origin="lower",
                    extent=(-2800, 2800, -1680, 1680),
                    cmap="gray_r",
                    vmin=0,
                    vmax=1.5,
                )
                ax.set_title("phase32 days" if index == 0 else "+30 s, wind240 m east")
            ax.set_xlabel("east (m)")
            ax.set_ylabel(("split" if split else "direct FP32") + " / south (m)")
    fig.suptitle(
        "Actual native cloud-density section at2782 m: sticking versus continuous wind"
    )
    fig.text(
        0.07,
        0.02,
        "Density section, not a sky/Place render. Middle column amplifies an individual frame's density change. Same field and physical coordinates.",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0.065, 1, 0.95))
    pdf.savefig(fig)
    plt.close(fig)


def costs(pdf, record):
    if not record["cost_cases"]:
        return
    captures = {row["name"]: row for row in record["captures"]}
    fig, ax = plt.subplots(figsize=(11.7, 6.5))
    labels, data = [], []
    for profile in (0, 1):
        for cover in (0.65, 1.0):
            for phase in (37.0, 2764800.0):
                for split in (False, True):
                    samples = [
                        sample["elapsed_ns"] / 1e6
                        for row in record["cost_cases"]
                        if (row["profile"], row["cover"], row["phase_s"], row["split"])
                        == (profile, cover, phase, split)
                        for batch in captures[row["name"]]["timing"]["runs"]
                        for sample in batch
                    ]
                    labels.append(
                        f"{'C' if profile else 'S'} {cover}\n{'32d' if phase > 100 else '37s'}\n{'split' if split else 'direct'}"
                    )
                    data.append(samples)
    ax.boxplot(data, tick_labels=labels, whis=(0, 100))
    ax.set_yscale("log")
    ax.set_ylabel("actual GPU dispatch (ms),1048576 density points")
    ax.set_title("Native microcost: three interleaved runs,32 timestamp pairs each")
    ax.grid(axis="y", alpha=0.2)
    fig.text(
        0.07,
        0.025,
        "Direct32-day baseline fails precision; speed does not grant quality parity.\n"
        "Isolated dispatches exclude a full frame, sky integration, history and device-profile budgets.",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0.08, 1, 1))
    pdf.savefig(fig)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    args = parser.parse_args()
    record = json.loads((args.folder / "receipt.json").read_text())
    with PdfPages(args.folder / "native-wind.pdf") as pdf:
        fields(pdf, record)
        motion(pdf, record, args.folder)
        costs(pdf, record)


if __name__ == "__main__":
    main()
