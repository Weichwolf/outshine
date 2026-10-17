"""Matched linear-radiance/coverage comparison after an actual alpha calibration."""

import argparse
import json
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def srgb(linear):
    linear = np.maximum(linear, 0)
    return np.where(
        linear <= 0.0031308, 12.92 * linear, 1.055 * linear ** (1 / 2.4) - 0.055
    )


def compare(root, output):
    calibration = json.loads(
        (root / "alpha-binary-calibration/report.json").read_text()
    )
    if calibration["stored_representation"] != "premultiplied":
        raise ValueError("actual premultiplied EXR calibration required")
    names = [
        ["full-physical-front-384-valid", "full-windows-front-384-valid"],
        ["full-physical-front-384-unshadowed", "full-windows-front-384-unshadowed"],
    ]
    frames, facts = [], []
    for row in names:
        images, reports = [], []
        for name in row:
            directory = root / name
            reports.append(json.loads((directory / "report.json").read_text()))
            with np.load(directory / "linear.npz", allow_pickle=False) as data:
                images.append(data["rgba"])
        if reports[0]["source_sha256"] != reports[1]["source_sha256"]:
            raise ValueError("identical saved physical owner/window input required")
        for key in (
            "view",
            "light_direction",
            "ortho_scale_m",
            "resolution",
        ):
            if reports[0]["scene"][key] != reports[1]["scene"][key]:
                raise ValueError("identical camera and lighting required")
        for report in reports:
            if "sun_shadows" in report["scene"]:
                if report["scene"]["sun_shadows"] != (len(frames) == 0):
                    raise ValueError("declared shadow countercase required")
            elif (
                report["producer"]["commit"]
                != "a501affd7236412ba0a1c4ce7432d40ea0d79280"
            ):
                raise ValueError(
                    "only the audited original default-shadow producer is supported"
                )
        frames.append(images)
        facts.append(reports)
    union = np.any(np.stack([a[..., 3] > 0.01 for row in frames for a in row]), axis=0)
    y, x = np.nonzero(union)
    if not len(x):
        raise ValueError("real positive source coverage required")
    lo = np.maximum([y.min() - 20, x.min() - 20], 0)
    hi = np.minimum([y.max() + 21, x.max() + 21], union.shape)
    region = np.s_[lo[0] : hi[0], lo[1] : hi[1]]
    fig, axes = plt.subplots(2, 3, figsize=(16, 9), dpi=160)
    metrics = []
    for row, (images, reports) in enumerate(zip(frames, facts, strict=True)):
        records = []
        for col, (rgba, report) in enumerate(zip(images, reports, strict=True)):
            alpha = rgba[..., 3]
            composite = rgba[..., :3] + (1 - alpha[..., None]) * 0.18
            axes[row, col].imshow(
                np.clip(srgb(composite[region]), 0, 1),
                origin="lower",
                interpolation="nearest",
            )
            mean = rgba[..., :3].sum(axis=(0, 1), dtype=np.float64) / alpha.sum(
                dtype=np.float64
            )
            axes[row, col].set_title(
                f"{report['representation']} | {'matched shadows' if row == 0 else 'shadows off'}\nalpha sum {alpha.sum():.2f}; conditional RGB {np.round(mean, 5)}",
                fontsize=10,
            )
            records.append(
                {
                    "representation": report["representation"],
                    "alpha_sum": float(alpha.sum(dtype=np.float64)),
                    "premultiplied_linear_rgb_sum": rgba[..., :3]
                    .sum(axis=(0, 1), dtype=np.float64)
                    .tolist(),
                    "conditional_linear_rgb_mean": mean.tolist(),
                }
            )
        difference = images[1][..., 3] - images[0][..., 3]
        axes[row, 2].imshow(
            difference[region],
            origin="lower",
            cmap="coolwarm",
            vmin=-1,
            vmax=1,
            interpolation="nearest",
        )
        axes[row, 2].set_title(
            "Coverage difference: window − physical\nshared native pixel grid; unchanged colour scale",
            fontsize=10,
        )
        metrics.append({"sun_shadows": row == 0, "members": records})
    for axis in axes.flat:
        axis.set_axis_off()
    fig.suptitle(
        "Same cached Picea needle interval | frontal light = view | full 256-needle window",
        fontsize=16,
    )
    fig.text(
        0.5,
        0.025,
        f"Actual 2560×1440 renders; shared magnified crop {hi[1] - lo[1]}×{hi[0] - lo[0]} pixels, source span384px. No physical-transport or whole-crown approval.",
        ha="center",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0.05, 1, 0.94))
    fig.savefig(output)
    plt.close(fig)
    result = {
        "actual_alpha_calibration": calibration,
        "cases": metrics,
        "native_crop_pixels": [lo.tolist(), hi.tolist()],
        "scope": "One real cached interval and frontal light; binding/representation diagnostic, not general optical/transport/LOD approval.",
    }
    output.with_suffix(".json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    compare(args.input, args.output)
