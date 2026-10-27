"""Review existing four-query C/T fields; no rendering, fits or new poses (#1167)."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from compare_mass_appearance import errors
from urban_parent_light import display

MODELS = ("original", "opaque-full", "full", "two")
LUMINANCE = np.array([0.2126, 0.7152, 0.0722])


def read_fields(path, shape):
    with np.load(path, allow_pickle=False) as archive:
        fields = {m: {k: archive[m + "__" + k] for k in ("C", "T")} for m in MODELS}
    for image in fields.values():
        if image["C"].shape != (*shape, 3) or image["T"].shape != (*shape, 1):
            raise ValueError("Raw C/T must have the original integer ROI shape")
        if not all(np.isfinite(v).all() for v in image.values()):
            raise ValueError("Finite saved pixel fields required")
        if np.any((image["T"] < 0) | (image["T"] > 1)):
            raise ValueError("Transmittance must be bounded")
    np.testing.assert_array_equal(fields["full"]["T"], fields["two"]["T"])
    return fields


def summary(image, original, origin):
    alpha, reference_alpha = 1 - image["T"], 1 - original["T"]
    delta = alpha - reference_alpha
    signal = image["C"] - 0.1 * alpha
    pixel_error = abs(signal - (original["C"] - 0.1 * reference_alpha)).max(axis=-1)
    y, x = np.unravel_index(pixel_error.argmax(), pixel_error.shape)
    return errors(image, original) | {
        "coverage_sum_px2": float(alpha.sum()),
        "coverage_excess_px2": float(np.maximum(delta, 0).sum()),
        "coverage_deficit_px2": float(np.maximum(-delta, 0).sum()),
        "premultiplied_luminance_sum": float((image["C"] @ LUMINANCE).sum()),
        "above_neutral_luminance_sum": float((signal @ LUMINANCE).sum()),
        "maximum_rgb_error_pixel": [int(origin[0] + x), int(origin[1] + y)],
    }


def review(center, filtered):
    one = json.loads((center / "report.json").read_text())
    integrated = json.loads((filtered / "report.json").read_text())
    if one["provenance"] != integrated["provenance"]:
        raise ValueError("The saved original/control provenance must match")
    if len(one["comparisons"]) != 4 or len(integrated["comparisons"]) != 4:
        raise ValueError("Exactly the existing four comparisons required")
    rows, panels, paths = [], [], [center / "report.json", filtered / "report.json"]
    for first, existing in zip(
        one["comparisons"], integrated["comparisons"], strict=True
    ):
        if first["query"] != existing["query"] or first["roi"] != existing["roi"]:
            raise ValueError(
                "Identical camera, hierarchy query and integer ROI required"
            )
        if first["sample_rate"] != 1 or existing["sample_rate"] != 16:
            raise ValueError("Existing center and 16x/32x records required")
        name = first["query"]["name"]
        box = np.asarray(first["roi"])
        shape = tuple((box[1] - box[0])[::-1])
        files = {
            1: center / (name + ".npz"),
            16: filtered / (name + ".npz"),
            32: filtered / (name + "-refined.npz"),
        }
        fields = {rate: read_fields(p, shape) for rate, p in files.items()}
        paths.extend(files.values())
        result = {"query": name, "roi": box.tolist(), "rates": {}}
        for rate, images in fields.items():
            stats = {m: summary(images[m], images["original"], box[0]) for m in MODELS}
            stats["two_vs_full"] = errors(images["two"], images["full"])
            result["rates"][rate] = stats
            if rate > 1:
                recorded = existing["errors" if rate == 16 else "refined_errors"]
                for model in MODELS[1:]:
                    if errors(images[model], images["original"]) != recorded[model]:
                        raise ValueError(
                            "Saved fields must reproduce their original recorded metrics"
                        )
        result["quadrature_16_to_32"] = {
            m: errors(fields[32][m], fields[16][m]) for m in MODELS
        }
        rows.append(result)
        panels.append((name, fields))
    return rows, panels, paths, integrated["provenance"]


def plots(panels, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(4, 6, figsize=(16, 10), layout="constrained")
    coverage, areas = plt.subplots(4, 4, figsize=(13, 10), layout="constrained")
    views = (
        (1, "original"),
        (16, "original"),
        (32, "original"),
        (32, "opaque-full"),
        (32, "full"),
        (32, "two"),
    )
    for row, (name, fields) in enumerate(panels):
        for col, (rate, model) in enumerate(views):
            image = fields[rate][model]
            axes[row, col].imshow(
                display(image["C"] + 0.1 * image["T"]), interpolation="nearest"
            )
            axes[row, col].set_title(f"{name}\n{model}, SS{rate}", fontsize=9)
            axes[row, col].set_axis_off()
        reference = 1 - fields[32]["original"]["T"][:, :, 0]
        for col, model in enumerate(("original", "opaque-full", "full")):
            alpha = 1 - fields[32][model]["T"][:, :, 0]
            areas[row, col].imshow(
                alpha, vmin=0, vmax=1, cmap="gray", interpolation="nearest"
            )
            areas[row, col].set_title(
                f"{name}: {model}\narea {alpha.sum():.6f} px²", fontsize=9
            )
        delta = 1 - fields[32]["full"]["T"][:, :, 0] - reference
        image = areas[row, 3].imshow(
            delta, vmin=-1, vmax=1, cmap="RdBu_r", interpolation="nearest"
        )
        areas[row, 3].set_title(
            "fractional minus original\nred excess / blue deficit", fontsize=9
        )
        for ax in areas[row]:
            ax.set_axis_off()
    figure.suptitle(
        "Existing same-query pixel fields, nearest enlarged crops; fixed neutral background0.1\nNo new renderer/pose/fit; finite SS32 is not an integral or quality certificate."
    )
    coverage.suptitle(
        "Same saved SS32 coverage; fixed [0,1] and signed [-1,1] scales\nFull/two coverage is exactly identical; unknown original sources remain required."
    )
    coverage.colorbar(
        image, ax=areas[:, 3], shrink=0.5, label="sampled coverage difference"
    )
    figure.savefig(output / "appearance.png", dpi=120)
    coverage.savefig(output / "coverage.png", dpi=120)
    plt.close(figure)
    plt.close(coverage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--center", type=Path, required=True)
    parser.add_argument("--filtered", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    rows, panels, paths, provenance = review(args.center, args.filtered)
    args.output.mkdir(parents=True, exist_ok=False)
    plots(panels, args.output)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "review_source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "read_only_inputs_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "provenance": provenance,
        "comparisons": rows,
        "scope": "Saved-data contrasts only; no rendering, fitting, new camera/light, quality tolerance or global/native/world/device admission. SS16/32 change is finite quadrature evidence, not an integral bound; contrasts are not additive causal attribution. Background0.1 only for diagnostic appearance; C/T remain separate.",
    }
    (args.output / "review.json").write_text(json.dumps(report, indent=2) + "\n")
    for row in rows:
        print(row["query"], json.dumps(row["rates"][32]["two"]), flush=True)


if __name__ == "__main__":
    main()
