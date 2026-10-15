"""Consume raw moments during actual cloud/air Scene/Depth raster composition."""

import argparse
import hashlib
import json
import os
import shlex
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from cloud_moment_raster_reference import judge

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def plot(output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    metadata = output / "moments-1/actual.declared-perspective.json"
    if not metadata.exists():
        return
    data = json.loads(metadata.read_text())
    images = np.fromfile(str(metadata)[:-5] + ".images.bin", "<f4").reshape(
        5, data["height"], data["width"], 4
    )
    weight, first = images[4, :, :, 0], images[4, :, :, 1]
    depth = np.divide(first, weight, out=np.zeros_like(first), where=weight > 0)
    fig, axes = plt.subplots(1, 3, figsize=(13, 4), constrained_layout=True)
    rgb = np.maximum(images[1, :, :, :3], 0)
    axes[0].imshow(rgb / (1 + rgb))
    axes[0].set_title("Actual cloud/air RGB; identical to old output\ndisplay x/(1+x)")
    for axis, value, title in (
        (axes[1], weight, "Native raw M0"),
        (axes[2], depth * 0.001, "Native finite M1/M0 (km)"),
    ):
        image = axis.imshow(value)
        axis.set_title(title)
        fig.colorbar(image, ax=axis, shrink=0.8)
    for axis in axes:
        axis.set_axis_off()
    fig.suptitle(
        f"Actual {data['width']}×{data['height']} Scene/Depth moment side product; no TAA claim"
    )
    fig.savefig(output / "cloud-moment-raster.pdf")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=43)
    parser.add_argument("--height", type=int, default=31)
    parser.add_argument("--scenario")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=True)
    receipt = output / "receipt.json"
    receipt.unlink(missing_ok=True)
    native = json.loads(args.native_receipt.read_text())
    binary = Path(native["binary"])
    if digest(binary) != native["binary_sha256"]:
        parser.error(
            "the reusable native raster binary differs from its producer receipt"
        )
    record = {
        "commands": [],
        "native_receipt_sha256": digest(args.native_receipt),
        "binary_sha256": digest(binary),
        "producer": native["producer_commit"],
    }

    def run(argv, name):
        argv = list(map(str, argv))
        result = subprocess.run(
            argv, cwd=ROOT, capture_output=True, text=True, timeout=180, check=False
        )
        (output / f"{name}.stdout").write_text(result.stdout)
        (output / f"{name}.stderr").write_text(result.stderr)
        record["commands"].append({"argv": argv, "exit_code": result.returncode})
        if result.returncode or result.stderr:
            raise RuntimeError(
                f"{name} failed or emitted diagnostics; inspect {output}"
            )
        return result.stdout

    shaders = native["shaders"].copy()
    for mode in (0, 1, 2):
        enabled = int(mode > 0)
        target = output / f"front-{mode}.spv"
        run(
            [
                *shlex.split(os.environ.get("GLSLANG", "glslangValidator")),
                "-V",
                "-Isrc",
                "-Isrc/render/shaders",
                "-Isrc/generators/weather/shaders",
                "-Itest/experiments/weather",
                *(f"-D{key}={value}" for key, value in native["parameters"].items()),
                f"-DCLOUD_AIR_MOMENTS={enabled}",
                f"-DCLOUD_VIEW_MOMENT_REPLAY={int(mode == 2)}",
                ROOT / "test/experiments/weather/cloud_view_opaque.frag",
                "-o",
                target,
            ],
            f"shader-{mode}",
        )
        shaders[3] = str(target)
        folder = output / f"moments-{mode}"
        folder.mkdir(exist_ok=True)
        run(
            [
                binary,
                *shaders,
                folder / "actual",
                args.width,
                args.height,
                *([args.scenario] if args.scenario else []),
            ],
            f"raster-{mode}",
        )
    rows = []
    for metadata in sorted((output / "moments-1").glob("*.json")):
        rows.append(judge(metadata, output / "moments-0" / metadata.name))
        direct = np.fromfile(str(metadata)[:-5] + ".images.bin", "<f4")
        replay = np.fromfile(
            str(output / "moments-2" / metadata.name)[:-5] + ".images.bin", "<f4"
        )
        np.testing.assert_array_equal(replay, direct)
        if (args.width, args.height) == (43, 31):
            previous = args.native_receipt.parent / metadata.name
            before = np.fromfile(str(previous)[:-5] + ".images.bin", "<f4")
            current = np.fromfile(
                str(output / "moments-0" / metadata.name)[:-5] + ".images.bin", "<f4"
            )
            np.testing.assert_array_equal(current, before)
    if len(rows) != (1 if args.scenario else 13):
        raise RuntimeError(
            "the actual native raster cohort must contain all13 existing cases"
        )
    record["cases"] = rows
    plot(output)
    record["commit"] = run(["git", "rev-parse", "HEAD"], "commit").strip()
    receipt.write_text(json.dumps(record, indent=2))
    print(json.dumps(rows, indent=2))


if __name__ == "__main__":
    main()
