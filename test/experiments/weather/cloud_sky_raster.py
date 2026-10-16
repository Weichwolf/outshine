"""Render and verify the explicitly selected native boundary-source sky consumer."""

import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from cloud_sky_reference import cross_cases, judge, read

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def plot(folder, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_pdf import PdfPages

    groups = (
        (
            "sky-clear",
            "sky-broken",
            "sky-broken-wind",
            "sky-only",
            "orbit",
            "space-outward",
        ),
        ("declared-perspective", "declared-ortho", "ortho-offset", "yaw-rotation"),
    )
    with PdfPages(output / "cloud-boundary-sky.pdf") as pdf:
        for names in groups:
            paths = [folder / f"actual.{name}.json" for name in names]
            paths = [path for path in paths if path.exists()]
            if not paths:
                continue
            fig, axes = plt.subplots(
                (len(paths) + 1) // 2,
                2,
                figsize=(12, 4 * ((len(paths) + 1) // 2)),
                squeeze=False,
                constrained_layout=True,
            )
            for axis, path in zip(axes.flat, paths, strict=False):
                case, images, _, _, _ = read(path)
                rgb = images[1, :, :3].reshape(case["height"], case["width"], 3)
                exposed = np.maximum(0, 4 * rgb)
                axis.imshow(exposed / (1 + exposed))
                axis.set_title(f"{case['name']} — {case['width']}×{case['height']}")
            for axis in axes.flat:
                axis.set_axis_off()
            fig.suptitle(
                "Actual native joint cloud/air sky; explicit boundary radiance\n"
                "Homogeneous-air source control; display4x/(1+4x); no Place/budget claim"
            )
            pdf.savefig(fig)
            plt.close(fig)
        if not list(folder.glob("actual.sky-*.json")):
            raise ValueError("a real sky capture is required for optical review")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-receipt", type=Path, required=True)
    parser.add_argument("--legacy-captures", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=43)
    parser.add_argument("--height", type=int, default=31)
    parser.add_argument("--scenario", default="all")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in system temp")
    if args.scenario == "all" and args.legacy_captures is None:
        parser.error("the full cohort requires original opaque captures")
    output.mkdir(parents=True, exist_ok=True)
    if list(output.glob("actual.*.json")):
        parser.error(
            "choose a fresh output directory; existing captures must not mix with this run"
        )
    receipt = output / "receipt.json"
    receipt.unlink(missing_ok=True)
    native = json.loads(args.native_receipt.read_text())
    binary = Path(native["binary"])
    if digest(binary) != native["binary_sha256"]:
        parser.error("native binary differs from its regular-archive producer receipt")
    if native["parameters"].get("CLOUD_VIEW_BOUNDARY_SKY") != 1:
        parser.error("the selected shader must declare boundary-source sky semantics")
    expected_sampling = {
        "CLOUD_VIEW_SAMPLES": 16,
        "CLOUD_LIGHT_SAMPLES": 6,
        "CLOUD_VIEW_WARP": 1,
    }
    if any(
        native["parameters"].get(key) != value
        for key, value in expected_sampling.items()
    ):
        parser.error(
            "this controlled reference requires the declared default16/6/warp1 sampling"
        )
    command = list(
        map(
            str,
            [
                binary,
                *native["shaders"],
                output / "actual",
                args.width,
                args.height,
                args.scenario,
                "boundary-sky",
            ],
        )
    )
    result = subprocess.run(
        command, cwd=ROOT, capture_output=True, text=True, timeout=180, check=False
    )
    (output / "actual.stdout").write_text(result.stdout)
    (output / "actual.stderr").write_text(result.stderr)
    if result.returncode or result.stderr:
        raise RuntimeError(
            f"actual native raster failed or emitted diagnostics: {output}"
        )
    rows = [judge(path) for path in sorted(output.glob("actual.*.json"))]
    if len(rows) != (26 if args.scenario == "all" else 1):
        raise AssertionError("all requested actual native cases must be present")
    cross = (
        cross_cases(output, args.legacy_captures) if args.scenario == "all" else None
    )
    plot(output, output)
    pixels = args.width * args.height
    row_bytes = (args.width * 16 + 255) // 256 * 256
    record = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "command": command,
        "exit_code": result.returncode,
        "validation_stderr_bytes": len(result.stderr),
        "native_receipt_sha256": digest(args.native_receipt),
        "binary_sha256": digest(binary),
        "shader_sha256": {path: digest(Path(path)) for path in native["shaders"]},
        "cases": rows,
        "cross_cases": cross,
        "capture_storage_bytes": {
            "texture_texels": 84 * pixels + 64,
            "readback_buffers": 5 * row_bytes * args.height,
            "readback_copy_per_frame": 5 * row_bytes * args.height,
            "capture_vectors_per_frame": 80 * pixels,
            "added_sky_targets_or_bindings": 0,
        },
        "gpu_time_ns": None,
        "environment_validation_layer_path": os.environ.get("VK_LAYER_PATH"),
    }
    receipt.write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({"cases": len(rows), "output": str(output)}))


if __name__ == "__main__":
    main()
