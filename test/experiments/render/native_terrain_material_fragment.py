"""Replay checked material sampling inputs through the actual fragment stage."""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from native_terrain_material_pyramid import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(
        output.is_relative_to(path)
        for path in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private fragment output belongs in system temp")
    receipt = json.loads((args.prepared / "receipt.json").read_text())
    for name in ("parameters.f32", "probes.bin", "expected.f32"):
        if (
            hashlib.sha256((args.prepared / name).read_bytes()).hexdigest()
            != receipt["input_sha256"][name]
        ):
            raise ValueError("checked source packet required")
    output.mkdir(parents=True, exist_ok=False)
    fields = np.fromfile(args.prepared / "parameters.f32", dtype="<u4")
    probes = np.fromfile(args.prepared / "probes.bin", dtype="<u4").reshape(-1, 8)
    indices = np.r_[np.arange(4096), len(probes) - 2, len(probes) - 1]
    fields[319] = len(fields)
    np.concatenate((fields, probes[indices].ravel())).tofile(output / "inputs.f32")
    expected = np.fromfile(args.prepared / "expected.f32", dtype="<u4").reshape(-1, 8)[
        indices
    ]
    commands = [
        run(
            [
                "nice",
                "-n",
                "10",
                "cmake",
                "--build",
                str(ROOT / "build"),
                "--target",
                "outshine_material_fragment_probe",
                "--parallel",
                str(os.cpu_count() or 1),
            ],
            output,
            "build",
        )
    ]
    compiler = next(
        line.split("=", 1)[1]
        for line in (ROOT / "build/CMakeCache.txt").read_text().splitlines()
        if line.startswith("OUTSHINE_GLSLANG:FILEPATH=")
    )
    vertex = ROOT / "src/render/shaders/fullscreen.vert"
    fragment = Path(__file__).with_name("terrain_material_pyramid_probe.frag")
    for name, source in (("vertex", vertex), ("fragment", fragment)):
        commands.append(
            run(
                [
                    compiler,
                    "-V",
                    "--target-env",
                    "vulkan1.0",
                    "-I" + str(ROOT / "src/render/shaders"),
                    str(source),
                    "-o",
                    str(output / (name + ".spv")),
                ],
                output,
                name,
            )
        )
    binary = ROOT / "build/outshine_material_fragment_probe"
    commands.append(
        run(
            [
                "env",
                "MTL_DEBUG_LAYER=1",
                str(binary),
                str(output / "vertex.spv"),
                str(output / "fragment.spv"),
                str(output / "inputs.f32"),
                str(output / "actual.f32"),
                str(len(indices)),
            ],
            output,
            "native",
        )
    )
    actual = np.fromfile(output / "actual.f32", dtype="<u4").reshape(-1, 8)
    np.testing.assert_array_equal(actual, expected)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "command": [sys.executable, *sys.argv],
        "commands": commands,
        "samples": len(indices),
        "invalid_query_controls": 2,
        "fragment_matches_float32_reference_bits": True,
        "native_log": (output / "native.log").read_text(),
        "compute_receipt_sha256": hashlib.sha256(
            (args.prepared / "receipt.json").read_bytes()
        ).hexdigest(),
        "source_sha256": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (
                Path(__file__),
                fragment,
                fragment.with_name("terrain_material_sampling.glsl"),
                ROOT / "test/harness/render/NativeMaterialFragmentProbe.cpp",
                binary,
            )
        },
        "limitations": [
            "Ordinary stored material values/addressing; existing extreme GGX failures remain separate.",
            "No world integration, TAA/motion, lighting or frame-time acceptance.",
        ],
    }
    (output / "receipt.json").write_text(json.dumps(report, indent=2) + "\n")
    print(output / "receipt.json")


if __name__ == "__main__":
    main()
