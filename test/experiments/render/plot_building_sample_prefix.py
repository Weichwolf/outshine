"""Private actual-native and same-source repeated-filter comparison figure."""

import argparse
import json
from pathlib import Path

import numpy as np
from building_sample_inputs import digest


def run(args):
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh private system-temp scientific figure required")
    report = json.loads((args.study / "report.json").read_text())
    controls = args.study / "prefix-filter-controls.npz"
    if digest(controls) != report["controls_sha256"]:
        raise ValueError("Actual prefix-filter controls changed")
    for path, expected in report["input_manifests"].items():
        if digest(Path(path) / "manifest.json") != expected:
            raise ValueError("Bound native source/opponent manifest changed")
    axis = max(r["jitter_axis"] for r in report["double_filter_controls"])
    names = [
        "original_ss64",
        "native_prefix_single",
        "native_prefix_ss64",
        f"python_prefix_jitter{axis}",
    ]
    with np.load(controls, allow_pickle=False) as saved:
        images = [saved[n] for n in names]
    original = images[0]
    rows = [
        [image[:, :, 3] for image in images],
        [image[:, :, :3].sum(2) for image in images],
        [abs(image[:, :, :3] - original[:, :, :3]).sum(2) for image in images],
    ]
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(3, 4, figsize=(12, 8), layout="constrained")
    titles = [
        "Native original SS64",
        "Native prefix, one pixel integral",
        "Native prefix, jitter64 again",
        f"Python same-source jitter{axis}",
    ]
    for row, pictures in enumerate(rows):
        maximum = 1 if row == 0 else max(float(p.max()) for p in pictures)
        for col, picture in enumerate(pictures):
            shown = axes[row, col].imshow(
                picture,
                origin="upper",
                extent=(576, 704, 408, 312),
                interpolation="nearest",
                cmap="gray" if row < 2 else "magma",
                vmin=0,
                vmax=maximum,
            )
            axes[row, col].set_xlim(622, 658)
            axes[row, col].set_ylim(376, 344)
            axes[row, col].set_title(titles[col], fontsize=8)
            axes[row, col].set_xlabel("native pixel x")
            axes[row, col].set_ylabel("native pixel y")
        figure.colorbar(
            shown,
            ax=axes[row],
            label=(
                "coverage",
                "scene-linear RGB sum",
                "absolute RGB-L1 versus original",
            )[row],
            shrink=0.8,
        )
    figure.suptitle(
        f"Existing512 capture-lit source, unchanged1280×720 query rays\nSame pixel window enlarged for inspection; originalSS64 is finite. {report['commit'][:9]}",
        fontsize=10,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(args.output, dpi=140)
    plt.close(figure)
    args.output.with_suffix(".json").write_text(
        json.dumps(
            {
                "study_commit": report["commit"],
                "controls_sha256": report["controls_sha256"],
                "figure_sha256": digest(args.output),
                "channel_arrays": names,
                "scope": "Scientific native-channel/error figure; fixed common scales and visual zoom only. No tone-map, world renderer or publication.",
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
