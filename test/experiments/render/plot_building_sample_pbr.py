"""Private scientific source/pixel calibration and view-only model comparison."""

import argparse
import json
from pathlib import Path

import numpy as np
from building_sample_inputs import digest


def run(args):
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh private system-temp figure required")
    report = json.loads((args.study / "report.json").read_text())
    for name, expected in report["output_sha256"].items():
        if digest(args.study / name) != expected:
            raise ValueError("Actual source/pixel control arrays changed")
    with np.load(args.study / "source-controls.npz", allow_pickle=False) as saved:
        source = dict(saved)
    with np.load(args.study / "pixel-controls.npz", allow_pickle=False) as saved:
        pixels = dict(saved)
    pairs = [
        (source["native"], source["capture"]),
        (source["capture"], source["perspective"]),
        (pixels["original"], pixels["perspective"]),
    ]
    titles = [
        (
            "Native capture source",
            "Existing GGX capture model",
            "Capture model − native |RGB|",
        ),
        (
            "GGX held capture view",
            "GGX perspective view",
            "View-only model |RGB change|",
        ),
        (
            "Native original SS64",
            "Pixel-integrated perspective model",
            "Model − original SS64 |RGB|",
        ),
    ]
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(3, 3, figsize=(12, 10), layout="constrained")
    for row, (first, second) in enumerate(pairs):
        pictures = [
            first[:, :, :3].sum(2),
            second[:, :, :3].sum(2),
            abs(first[:, :, :3] - second[:, :, :3]).sum(2),
        ]
        maximum = max(float(p.max()) for p in pictures[:2])
        for col, picture in enumerate(pictures):
            extent = (0, 512, 512, 0) if row < 2 else (576, 704, 408, 312)
            shown = axes[row, col].imshow(
                picture,
                extent=extent,
                origin="upper",
                interpolation="nearest",
                cmap="gray" if col < 2 else "magma",
                vmin=0,
                vmax=maximum if col < 2 else float(picture.max()),
            )
            axes[row, col].set_title(titles[row][col], fontsize=9)
            axes[row, col].set_xlabel("source cell x" if row < 2 else "native pixel x")
            axes[row, col].set_ylabel("source cell y" if row < 2 else "native pixel y")
            if row == 2:
                axes[row, col].set_xlim(622, 658)
                axes[row, col].set_ylim(376, 344)
            if col != 0:
                figure.colorbar(
                    shown,
                    ax=axes[row, col],
                    shrink=0.8,
                    label="scene-linear RGB sum"
                    if col == 1
                    else "absolute RGB-L1; row-specific scale",
                )
    figure.suptitle(
        f"All43049 stored points, fixed geometry/material/coverage;1280×720 query rays\nCapture-calibrated model counterfactual, no independent native perspective-PBR oracle. {report['commit'][:9]}",
        fontsize=10,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(args.output, dpi=130)
    plt.close(figure)
    args.output.with_suffix(".json").write_text(
        json.dumps(
            {
                "study_commit": report["commit"],
                "figure_sha256": digest(args.output),
                "controls_sha256": report["output_sha256"],
                "scope": "Private scientific source/pixel comparison, row-specific RGB error scales explicitly labelled. Pixel image visually enlarged without changing data/profile/rays. No world renderer or published gallery.",
            },
            indent=2,
        )
        + "\n"
    )
    print(json.dumps({"figure": str(args.output)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--study", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
