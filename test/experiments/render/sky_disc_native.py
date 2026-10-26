"""Render actual solar-disc shaders before/after with a GPU-generated Veil LUT."""

import argparse
import json
import os
import shlex
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "weather"))
from cloud_sun_reuse_native import ROOT, build_consumer, digest
from sky_disc_validation import analyze


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--api-root", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--archive-receipt", type=Path, required=True)
    parser.add_argument("--baseline", required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private captures belong outside the checkout")
    args.output.mkdir(parents=True, exist_ok=True)
    record = {"commands": []}

    def run(command, folder, name, cwd=ROOT):
        command = list(map(str, command))
        start = time.monotonic()
        result = subprocess.run(
            command, cwd=cwd, capture_output=True, text=True, check=False
        )
        (folder / f"{name}.stdout").write_text(result.stdout)
        (folder / f"{name}.stderr").write_text(result.stderr)
        record["commands"].append(
            {
                "argv": command,
                "cwd": str(cwd),
                "exit": result.returncode,
                "process_io_wall_seconds": time.monotonic() - start,
            }
        )
        (args.output / "receipt.json").write_text(json.dumps(record, indent=2))
        result.check_returncode()
        return result.stdout

    record["commit"] = run(["git", "rev-parse", "HEAD"], args.output, "commit").strip()
    record["dirty"] = run(
        ["git", "status", "--porcelain"], args.output, "status"
    ).strip()
    baseline = run(
        ["git", "rev-parse", "--verify", args.baseline], args.output, "baseline"
    ).strip()
    changed = run(
        [
            "git",
            "diff",
            "--name-only",
            baseline,
            "--",
            "src/render/shaders",
            "src/world/sky",
        ],
        args.output,
        "common-source-check",
    ).splitlines()
    if changed != ["src/render/shaders/sky.frag"]:
        raise RuntimeError(
            "baseline comparison requires unchanged common shader/medium sources"
        )
    record["baseline"] = baseline
    before = args.output / "before-sky.frag"
    before.write_text(
        run(
            ["git", "show", baseline + ":src/render/shaders/sky.frag"],
            args.output,
            "baseline-source",
        )
    )
    api = args.api_root.resolve()
    binary, record["consumer"] = build_consumer(
        args,
        run,
        ROOT / "test/experiments/render/sky_disc_native.cpp",
        [
            api / "test/harness/render",
            api / "src/render/device/native",
            api / "test/harness/shared",
        ],
    )
    glslang = shlex.split(os.environ.get("GLSLANG", "glslangValidator"))
    for name, source in (
        ("vertex", ROOT / "src/render/shaders/fullscreenNdc.vert"),
        ("veil", ROOT / "src/render/shaders/mediumTransmittance.comp"),
        ("before", before),
        ("after", ROOT / "src/render/shaders/sky.frag"),
    ):
        run(
            [
                "nice",
                "-n",
                "10",
                *glslang,
                "-V",
                "-Isrc",
                "-Isrc/render/shaders",
                source,
                "-o",
                args.output / f"{name}.spv",
            ],
            args.output,
            "compile-" + name,
        )
    promoted = args.output / "promoted-veil.rgba32"
    for storage in ("f16", "f32"):
        for state in ("before", "after"):
            folder = args.output / f"{state}-{storage}"
            folder.mkdir(exist_ok=True)
            command = [
                binary,
                args.output / "vertex.spv",
                args.output / f"{state}.spv",
                args.output / "veil.spv",
                folder,
            ]
            if storage == "f32":
                command.append(promoted)
            run(command, folder, "native")
            if storage == "f16" and state == "before":
                np.fromfile(folder / "veil.rgba16", "<f2").astype("<f4").tofile(
                    promoted
                )
    record["analysis"] = analyze(args.output)
    record["gpu_time"] = (
        "unmeasured: the paired archive exposes dispatch scope, not raster/submission timestamps"
    )
    paths = [
        ROOT / "src/render/shaders/sky.frag",
        *Path(__file__).parent.glob("sky_disc*"),
        Path(__file__).with_name("test_sky_disc_reference.py"),
    ]
    record["source_hashes"] = {
        str(p.relative_to(ROOT)): digest(p) for p in paths if p.is_file()
    }
    record["capture_hashes"] = {
        str(p.relative_to(args.output)): digest(p)
        for p in args.output.rglob("*")
        if p.suffix in (".rgba32", ".velocity32", ".parameters", ".rgba16", ".spv")
    }
    (args.output / "receipt.json").write_text(json.dumps(record, indent=2))
    print("Native solar-disc comparison passed:", args.output / "receipt.json")


if __name__ == "__main__":
    main()
