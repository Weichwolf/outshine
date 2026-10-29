"""Private scientific comparison of actual native pixels and held-field controls."""

import argparse
import json
from pathlib import Path

import numpy as np
from building_sample_inputs import digest, readbacks


def inputs(study):
    report = json.loads((study / "report.json").read_text())
    prefix = study / "prefix-controls.npz"
    if digest(prefix) != report["prefix_controls_sha256"]:
        raise ValueError("Actual prefix control arrays changed")
    for directory, expected in report["input_npz"].items():
        if digest(Path(directory) / "captures.npz") != expected:
            raise ValueError("Actual native comparison input changed")
    primary = [
        Path(p)
        for p in report["input_npz"]
        if Path(p).name not in ("same-rays", "high-sampling")
    ]
    high = [Path(p) for p in report["input_npz"] if Path(p).name == "high-sampling"]
    if len(primary) != 1 or len(high) != 1:
        raise ValueError("One actual primary and higher-reference input required")
    manifest, values = readbacks(high[0])
    lower_manifest, lower = readbacks(primary[0])
    query = manifest["cases"][0]["query"]
    matching = [
        i
        for i, c in enumerate(lower_manifest["cases"])
        if c["query"]["name"] == query["name"]
    ]
    if len(matching) != 1:
        raise ValueError("Same native original query required")
    axis = query["filter_axis_samples"]
    low_axis = lower_manifest["cases"][matching[0]]["query"]["filter_axis_samples"]
    original = values[f"case0_original_ss{axis**2}"].astype(float)
    sample = values[f"case0_samples_ss{axis**2}"].astype(float)
    original_low = lower[f"case{matching[0]}_original_ss{low_axis**2}"].astype(float)
    with np.load(prefix, allow_pickle=False) as arrays:
        held = arrays["high_case0_prefix"]
    if original.shape != held.shape:
        raise ValueError("Same stored native pixel crop required")
    return report, manifest, (original, sample, held), original_low


def run(args):
    if args.output.exists() or not args.output.resolve().is_relative_to(
        Path("/tmp").resolve()
    ):
        raise ValueError("Fresh private system-temp scientific figure required")
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    report, manifest, images, original_low = inputs(args.study)
    original, sample, held = images
    low, high = np.asarray(manifest["crop"]).reshape(2, 2)
    extent = (low[0], high[0], high[1], low[1])
    rows = [
        [image[:, :, 3] for image in images],
        [image[:, :, :3].sum(2) for image in images],
        [
            abs(image[:, :, :3] - original[:, :, :3]).sum(2)
            for image in (original_low, sample, held)
        ],
    ]
    titles = [
        [
            "Native original",
            "Native stored-sample quads",
            "Existing held-radiance prefix",
        ],
        [
            "Original scene-linear RGB sum",
            "Samples scene-linear RGB sum",
            "Held prefix scene-linear RGB sum",
        ],
        [
            "Original reference:4² versus64²",
            "Samples versus original64²",
            "Held prefix versus original64²",
        ],
    ]
    figure, axes = plt.subplots(3, 3, figsize=(12, 9), layout="constrained")
    for row, values in enumerate(rows):
        maximum = 1 if row == 0 else max(float(value.max()) for value in values)
        for column, value in enumerate(values):
            shown = axes[row, column].imshow(
                value,
                origin="upper",
                extent=extent,
                interpolation="nearest",
                cmap="gray" if row < 2 else "magma",
                vmin=0,
                vmax=maximum,
            )
            axes[row, column].set_title(titles[row][column], fontsize=9)
            axes[row, column].set_xlabel("native pixel x")
            axes[row, column].set_ylabel("native pixel y")
        figure.colorbar(
            shown,
            ax=axes[row],
            label=(
                "coverage"
                if row == 0
                else "scene-linear RGB sum"
                if row == 1
                else "absolute RGB L1 per native pixel"
            ),
            shrink=0.8,
        )
    figure.suptitle(
        "128-point capture →32px perspective; finite native64² reference\nFull raster1280×720, unchanged rays in the stored crop; held canonical radiance is not query-relit",
        fontsize=11,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(args.output, dpi=130)
    plt.close(figure)
    receipt = {
        "study_report_sha256": digest(args.study / "report.json"),
        "figure_sha256": digest(args.output),
        "study_commit": report["commit"],
        "native_profile": manifest["raster"],
        "shown_crop": manifest["crop"],
        "scope": "Scientific native-buffer/channel/error visualization with explicit shared scales; no rendered World, continuous oracle, PBR relighting or pose admission.",
    }
    args.output.with_suffix(".json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({"output": str(args.output), "study_commit": report["commit"]}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--study", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
