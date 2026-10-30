"""Private native/plane comparison and signed coverage-component figures."""

import argparse
import json
from pathlib import Path

import numpy as np
from building_sample_inputs import digest


def comparison(report, arrays, plt):
    names = (
        "high0_original",
        "native_flat_linear",
        "high0_fitted",
        "native_depth_ss64",
    )
    images = [arrays[n] for n in names]
    titles = (
        "Native original SS64",
        "Native capture-facing flat prefix",
        "Fitted single plane / Python",
        "Native depth-preserving sample quads",
    )
    figure, axes = plt.subplots(3, 4, figsize=(12, 8), layout="constrained")
    for row in range(3):
        values = [
            im[:, :, 3]
            if row == 0
            else im[:, :, :3].sum(2)
            if row == 1
            else abs(im[:, :, :3] - images[0][:, :, :3]).sum(2)
            for im in images
        ]
        maximum = 1 if row == 0 else max(float(v.max()) for v in values)
        for col, value in enumerate(values):
            shown = axes[row, col].imshow(
                value,
                origin="upper",
                extent=(576, 704, 408, 312),
                interpolation="nearest",
                cmap="gray" if row < 2 else "magma",
                vmin=0,
                vmax=maximum,
            )
            axes[row, col].set_title(titles[col], fontsize=8)
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
        f"Capture coordinates fixed; support plane fitted from all43049 source depths\nUnchanged1280×720 query rays. Fitted carrier is Python only, no native/World admission. {report['commit'][:9]}",
        fontsize=10,
    )
    return figure, axes


def components(report, arrays, plt):
    names = (
        "pixel_reconstruction",
        "capture_predicate",
        "point_discretization",
        "flat_projection",
        "geos_native_reference",
        "native_combined",
    )
    values = [arrays["stage_" + n] for n in names]
    titles = (
        "Native pixel vs Float64 integral",
        "Native vs GEOS capture predicates",
        "Point raster vs continuous capture",
        "Flat capture vs original projection",
        "GEOS original vs native SS64",
        "Combined native prefix − originalSS64",
    )
    figure, axes = plt.subplots(2, 3, figsize=(12, 7), layout="constrained")
    maximum = max(float(abs(v).max()) for v in values)
    for ax, value, title in zip(axes.flat, values, titles, strict=True):
        shown = ax.imshow(
            value,
            origin="upper",
            extent=(576, 704, 408, 312),
            interpolation="nearest",
            cmap="RdBu_r",
            vmin=-maximum,
            vmax=maximum,
        )
        ax.set_title(title, fontsize=9)
    figure.colorbar(
        shown,
        ax=axes.ravel().tolist(),
        label="signed coverage difference; one common scale",
        shrink=0.8,
    )
    figure.suptitle(
        f"Existing GEOS/native coverage controls, unchanged1280×720 rays\nSigned maps add per pixel; their L1 norms do not. No certified native/World error bound. {report['commit'][:9]}",
        fontsize=10,
    )
    return figure, axes


def run(args):
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh private system-temp scientific figure required")
    report = json.loads((args.study / "report.json").read_text())
    path = args.study / "plane-controls.npz"
    if digest(path) != report["controls_sha256"]:
        raise ValueError("Actual plane control arrays changed")
    for directory, expected in report["input_manifests"].items():
        if digest(Path(directory) / "manifest.json") != expected:
            raise ValueError("Actual native/source manifest binding changed")
    with np.load(path, allow_pickle=False) as saved:
        arrays = dict(saved)
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = (components if args.components else comparison)(report, arrays, plt)
    for ax in axes.flat:
        ax.set_xlim(622, 658)
        ax.set_ylim(376, 344)
        ax.set_xlabel("native pixel x")
        ax.set_ylabel("native pixel y")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(args.output, dpi=130)
    plt.close(figure)
    args.output.with_suffix(".json").write_text(
        json.dumps(
            {
                "study_commit": report["commit"],
                "figure_sha256": digest(args.output),
                "controls_sha256": report["controls_sha256"],
                "kind": "components" if args.components else "comparison",
                "scope": "Actual source/native/field pixels, fixed common scales and visual crop enlargement only; scientific diagnostic, no published gallery or native fitted-plane run.",
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
    parser.add_argument("--components", action="store_true")
    run(parser.parse_args())
