"""Sample stored NPOT pages through the shared native R8 texture path."""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from native_terrain_litter_field import capture
from native_terrain_material_pyramid import ROOT, run
from terrain_litter_field import LitterField


def bilinear(pixels, uv):
    at = uv.astype(float) * len(pixels) - 0.5
    first = np.floor(at).astype(int)
    fraction = at - first
    low, high = (
        np.clip(first, 0, len(pixels) - 1),
        np.clip(first + 1, 0, len(pixels) - 1),
    )
    a = pixels[low[:, 1], low[:, 0]].astype(float)
    b = pixels[low[:, 1], high[:, 0]].astype(float)
    c = pixels[high[:, 1], low[:, 0]].astype(float)
    d = pixels[high[:, 1], high[:, 0]].astype(float)
    return (
        (a * (1 - fraction[:, 0]) + b * fraction[:, 0]) * (1 - fraction[:, 1])
        + (c * (1 - fraction[:, 0]) + d * fraction[:, 0]) * fraction[:, 1]
    ) / 255


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pages", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(
        output.is_relative_to(p)
        for p in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private native outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=False)
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
    for name, source in (
        ("vertex", ROOT / "src/render/shaders/fullscreen.vert"),
        ("fragment", Path(__file__).with_name("terrain_litter_texture_probe.frag")),
    ):
        commands.append(
            run(
                [
                    compiler,
                    "-V",
                    "--target-env",
                    "vulkan1.0",
                    str(source),
                    "-o",
                    str(output / (name + ".spv")),
                ],
                output,
                name,
            )
        )
    source_receipt = args.pages / "receipt.json"
    records = []
    rng = np.random.default_rng(1934)
    for core in (64, 128):
        for level in (0, 5):
            path = args.pages / f"case-1-{core}-hit/mip-{level}.r8"
            side = core + 2
            pixels = np.fromfile(path, dtype=np.uint8).reshape(side, side)
            field = LitterField((0, 0), 1, 1, (pixels,))
            x, y = np.meshgrid(np.arange(side), np.arange(side))
            centres = np.column_stack((x.ravel(), y.ravel()))
            uv = (centres + 0.5) / side
            queries = np.zeros((len(uv) + 512, 12), dtype="<f4")
            queries[: len(uv), :2] = uv
            queries[len(uv) :, :2] = rng.uniform(-0.05, 1.05, (512, 2))
            results = []
            for batch, first in enumerate(range(0, len(queries), 16000)):
                results.append(
                    capture(
                        output,
                        f"page-{core}-{level}-{batch}",
                        field,
                        queries[first : first + 16000],
                        commands,
                    )
                )
            native = np.concatenate(results)
            expected = pixels.ravel().astype(float) / 255
            error = float(np.max(np.abs(native[: len(expected), 0] - expected)))
            if error > 8 * np.finfo(np.float32).eps:
                raise ValueError("stored NPOT page texel centres differ on the GPU")
            if np.any(native[:, 0] < 0) or np.any(native[:, 0] > 1):
                raise ValueError("sampled R8 page left its unorm range")
            interpolated = bilinear(pixels, queries[len(expected) :, :2])
            records.append(
                {
                    "core_texels": core,
                    "level": level,
                    "queries": len(queries),
                    "centre_error": error,
                    "bilinear_observed_max_error": float(
                        np.max(np.abs(native[len(expected) :, 0] - interpolated))
                    ),
                    "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                }
            )
    (output / "receipt.json").write_text(
        json.dumps(
            {
                "commit": subprocess.check_output(
                    ["git", "rev-parse", "HEAD"], text=True
                ).strip(),
                "source_sha256": {
                    str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in (
                        Path(__file__),
                        ROOT / "src/render/device/R8FieldTexture.cpp",
                        ROOT / "src/render/device/R8FieldTexture.h",
                        ROOT / "test/harness/render/NativeMaterialFragmentProbe.cpp",
                        ROOT / "test/harness/render/NativeMaterialCaptureTexture.h",
                        ROOT / "build/outshine_material_fragment_probe",
                    )
                },
                "command": [sys.executable, *sys.argv],
                "commands": commands,
                "page_receipt_sha256": hashlib.sha256(
                    source_receipt.read_bytes()
                ).hexdigest(),
                "records": records,
                "limitations": [
                    "Native page sampling and replay only; device is recorded in probe logs, no World/atlas/frame budget acceptance.",
                    "Fractional sampling errors are observed, not a universal hardware precision bound.",
                ],
            },
            indent=2,
        )
        + "\n"
    )
    print(output / "receipt.json")


if __name__ == "__main__":
    main()
