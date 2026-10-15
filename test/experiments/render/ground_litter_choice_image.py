"""Compare actual old/new native ground albedo on oblique affine footprints."""

import argparse
import hashlib
import json
import shutil
import tempfile
from pathlib import Path

import numpy as np
from ground_litter_native import capture
from ground_litter_reference import DTYPE, ROOT
from PIL import Image, ImageDraw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--shaders", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use a directory in system temp")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    palette = np.fromfile(
        ROOT / "test/outshine/data/materials/ground-litter.palette", dtype="<f4"
    )
    palette.tofile(output / "native.palette")
    rows = int(palette[:1].view("<u4")[0])
    surfaces = 4 * (rows + 2) + rows + 1
    x, y = np.meshgrid(np.arange(64) - 32, np.arange(48) - 24)
    probes = []
    for row in (0, 1, 3):
        scale = float(palette[surfaces + 8 * row + 2])
        dx = scale * np.array([1, 1]) / np.sqrt(2)
        dy = scale * 0.125 * np.array([-1, 1]) / np.sqrt(2)
        points = np.array([0.31, 0.47]) * scale + x[..., None] * dx + y[..., None] * dy
        records = np.zeros(x.size, dtype=DTYPE)
        records["point_dx"][:, :2] = points.reshape(-1, 2)
        records["point_dx"][:, 2:] = dx
        records["dy"] = dy
        records["which"], records["mode"] = row, 1
        probes.append(records)
    probes = np.concatenate(probes)
    probes.tofile(output / "probes.bin")
    for name, source in [(0, "0-probe.spv"), (3, "default.spv")]:
        shutil.copyfile(args.shaders / source, output / f"filter-{name}.spv")
    before, after = [
        capture(args.binary.resolve(), output, mode, "native", len(probes), rows - 1)
        for mode in (0, 3)
    ]
    canvas = Image.new("RGB", (512, 672), "white")
    draw = ImageDraw.Draw(canvas)
    draw.text(
        (4, 4),
        "Native ground albedo, 64x48 affine pixels enlarged 4x; no Place claim.",
        fill="black",
    )
    for row, material in enumerate((0, 1, 3)):
        for col, (label, values) in enumerate(
            [("former box", before), ("production nine points", after)]
        ):
            draw.text(
                (col * 256 + 4, row * 216 + 24),
                f"{label}, palette row {material}",
                fill="black",
            )
            linear = np.maximum(
                values[row * x.size : (row + 1) * x.size, :3].reshape(48, 64, 3), 0
            )
            srgb = np.where(
                linear <= 0.0031308, 12.92 * linear, 1.055 * linear ** (1 / 2.4) - 0.055
            )
            pixels = Image.fromarray(np.uint8(np.clip(srgb, 0, 1) * 255)).resize(
                (256, 192), Image.Resampling.NEAREST
            )
            canvas.paste(pixels, (col * 256, row * 216 + 44))
    canvas.save(output / "albedo.png")
    report = {
        "scope": __doc__,
        "cases": len(probes),
        "gpu_executed": True,
        "max_rgb_change": float(np.max(np.abs(after[:, :3] - before[:, :3]))),
        "default_shader_sha256": hashlib.sha256(
            (output / "filter-3.spv").read_bytes()
        ).hexdigest(),
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
