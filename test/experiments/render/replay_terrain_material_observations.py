"""Compare exact material alternatives on explicitly bound instrumented GPU inputs."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from terrain_material_capture import load
from terrain_material_observations import evaluate_capture

METHODS = (
    ("current_analytic", False, False),
    ("support_selected_analytic", False, True),
    ("singleton_word_lookup", True, True),
)


def plots(rows, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(
        len(rows), 3, figsize=(12, 3.1 * len(rows)), squeeze=False
    )
    for index, row in enumerate(rows):
        values = np.load(output / row["arrays"])
        error = values["reference_minus_gpu"]
        for axis, group, title, unit in zip(
            axes[index],
            ((0, 1, 2), (3,), (4, 5)),
            ("Albedo", "Roughness", "Height derivatives"),
            ("absolute linear RGB", "absolute roughness", "metres per pixel"),
            strict=True,
        ):
            for channel in group:
                axis.plot(np.abs(error[:, channel]), label=str(channel))
            axis.set(
                xlabel="Native sample index",
                ylabel=unit,
                title=row["scene"] + ": " + title,
            )
            axis.grid(alpha=0.2)
    figure.suptitle(
        "Ideal Double reference minus instrumented binary32 observations\nAll three Python alternatives are exactly equal; no acceptance threshold"
    )
    figure.tight_layout()
    figure.savefig(output / "instrumented-observation-errors.png", dpi=130)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    reports = []
    for index, path in enumerate(args.input):
        capture = load(path)
        point = capture.points
        observed = np.column_stack((point[:, 36:40], point[:, 40:42]))
        arrays = {
            "original_points": point,
            "palette_words_u32": capture.palette_words,
            "instrumented_gpu": observed,
        }
        reference = None
        methods = {}
        for name, prepared, selected in METHODS:
            result, work = evaluate_capture(
                capture, prepared=prepared, prune_support=selected
            )
            if reference is None:
                reference = result
            elif result.tobytes() != reference.tobytes():
                raise ValueError("exact full material/derivative equality failed")
            arrays[name] = result
            methods[name] = {"work": work, "returned_array_bytes": result.nbytes}
        error = reference - observed
        arrays["reference_minus_gpu"] = error
        array_name = f"{index}-observations.npz"
        np.savez_compressed(args.output / array_name, **arrays)
        worst = np.unravel_index(abs(error).argmax(), error.shape)
        reports.append(
            {
                "scene": capture.metadata["scene"],
                "input": str(path),
                "input_sha256": capture.input_sha256,
                "native_product_commit": capture.metadata["product_commit"],
                "frame_index": capture.metadata["frame_index"],
                "point_count": len(point),
                "palette_sha256": capture.metadata["palette_sha256"],
                "original_unnormalized_surface_normals_preserved": True,
                "exact_python_alternatives": True,
                "methods": methods,
                "arrays": array_name,
                "native_observation_channel_max_abs": np.max(
                    abs(error), axis=0
                ).tolist(),
                "native_observation_channel_rms": np.sqrt(
                    np.mean(error * error, axis=0)
                ).tolist(),
                "worst_observation_residual": {
                    "index": int(worst[0]),
                    "channel": int(worst[1]),
                    "signed_value": float(error[worst]),
                    "native_pixel": point[worst[0], :2].tolist(),
                },
                "observation_limitations": list(capture.metadata["limitations"]),
            }
        )
        print(
            json.dumps(
                {
                    "scene": capture.metadata["scene"],
                    "state": "all_python_alternatives_exact",
                    "samples": len(point),
                }
            ),
            flush=True,
        )
    plots(reports, args.output)
    source_names = (
        "terrain_material_capture.py",
        "terrain_material_observations.py",
        "terrain_material_noise.py",
        "terrain_material_lattice.py",
        "terrain_material_plane.py",
        "terrain_material_substrate.py",
        "terrain_material_litter.py",
        "probe_terrain_material_substrate.py",
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in source_names
        },
        "scope": "Actual bound A18 instrumented input observations, ideal Double current material algebra with captured class/rock weights. Six material/height-derivative channels, not a full renderer or replay of class selection. Native residuals are observations, never approved tolerances. No native output, GPU/frame/p99, full-world residency or NDF improvement claim. Singleton fields are constructed inside this quality control; their cumulative bytes are neither prepared-hit costs nor a world working set. Final shading normals are not independently revalidated: exact position derivatives were not captured.",
        "rows": reports,
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
