"""Render the same native cached wall through the actual public Engine MR path."""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
from facade_coating_raster import ROOT
from masonry_relief_raster import sources


def compile_family(runtime, baseline, enabled):
    source = sources(runtime / "source", enabled, normal_export=False)
    target = runtime / "build/shaders"
    target.mkdir(parents=True, exist_ok=True)
    shutil.copytree(baseline / "build/shaders", target, dirs_exist_ok=True)
    shaders = sorted(target.glob("lit-*.frag.spv"))
    assert len(shaders) == 252
    with (runtime / "compile.log").open("w") as log:
        for path in shaders:
            variant = path.name.removeprefix("lit-").removesuffix(".frag.spv")
            defines = {
                "LIT_KIND": variant[0],
                "LIT_TEXTURED": variant[1],
                "LIT_MAPPED": variant[2],
                "SUBJECT_WRITES_VELOCITY": variant[4],
                "SUBJECT_NORMAL_LOCATION": variant[5],
                "SUBJECT_IDENTITY_LOCATION": variant[6],
                "SUBJECT_BASE_LOCATION": variant[7] if len(variant) > 7 else "0",
                "SUBJECT_METALROUGH_LOCATION": variant[8] if len(variant) > 8 else "0",
            }
            cmd = [
                "nice",
                "-n",
                "10",
                os.environ.get("GLSLANG", "glslangValidator"),
                "-V",
                "--target-env",
                "vulkan1.0",
                "-S",
                "frag",
                "-Isrc",
                "-Isrc/render/shaders",
                "-Isrc/content/shade",
                "-Ibuild/shaders",
                *[f"-D{k}={v}" for k, v in defines.items()],
                str(source),
                "-o",
                str(path),
            ]
            subprocess.run(
                cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True
            )
    return {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in shaders}


def chart(output, images):
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    figure, axes = plt.subplots(2, 2, figsize=(16, 10), constrained_layout=True)
    for col, image in enumerate(images):
        axes[0, col].imshow(image)
        axes[0, col].set_title("Before" if col == 0 else "Fused2mm bevel prototype")
        axes[1, col].imshow(image[200:520, 320:960], interpolation="nearest")
        axes[1, col].set_title("Identical native crop (320,200)-(960,520)")
    for ax in axes.ravel():
        ax.axis("off")
    figure.suptitle(
        "Public Engine /1280×720 /same native cache wall, MR recipe, camera and light"
    )
    figure.savefig(output / "comparison.pdf")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--baseline-checkout", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(ROOT):
        parser.error("use an explicit system-temp output outside the checkout")
    output.mkdir(parents=True, exist_ok=True)
    records, images, linear = [], [], []
    from matplotlib import pyplot as plt

    for name, enabled in (("before", False), ("after", True)):
        runtime = output / name
        runtime.mkdir(exist_ok=True)
        if not (runtime / "src").exists():
            (runtime / "src").symlink_to(ROOT / "src", target_is_directory=True)
        hashes = compile_family(runtime, args.baseline_checkout.resolve(), enabled)
        prefix = runtime / "scene"
        cmd = [
            str(args.binary.resolve()),
            str(args.fixture.resolve()),
            str(prefix),
            "2",
        ]
        began = time.monotonic()
        result = subprocess.run(
            cmd, cwd=runtime, capture_output=True, text=True, check=False
        )
        text = result.stdout + result.stderr
        (runtime / "capture.log").write_text(text)
        assert result.returncode == 0 and re.search(
            r"CHECKS \d+ FAILURES 0 SKIPPED 0 UNPREPARED 0 PARTIAL 0", text
        )
        values = np.fromfile(prefix.with_suffix(".f32"), dtype="<f4").reshape(
            720, 1280, 4
        )
        assert np.isfinite(values).all()
        linear.append(values)
        images.append(plt.imread(prefix.with_suffix(".png")))
        records.append(
            {
                "name": name,
                "command": cmd,
                "cwd": str(runtime),
                "exit": result.returncode,
                "whole_process_seconds": time.monotonic() - began,
                "fragment_hashes": hashes,
                "linear_sha256": hashlib.sha256(
                    prefix.with_suffix(".f32").read_bytes()
                ).hexdigest(),
            }
        )
    difference = np.abs(linear[1] - linear[0])
    assert difference.max() > 0
    report = {
        "records": records,
        "changed_pixels": int(
            np.count_nonzero(np.any(difference[:, :, :3] > 0, axis=-1))
        ),
        "linear_rgb_max_change": float(difference[:, :, :3].max()),
        "linear_rgb_mean_change": float(difference[:, :, :3].mean()),
        "binary_sha256": hashlib.sha256(args.binary.resolve().read_bytes()).hexdigest(),
        "fixture_sha256": hashlib.sha256(args.fixture.read_bytes()).hexdigest(),
        "scope": "Actual lit public-API material crop; no Place/current-world/frame claim",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    chart(output, images)
    print({k: v for k, v in report.items() if k not in ("records",)})


if __name__ == "__main__":
    main()
