"""Render affine wall albedo charts with the actual native facade filter."""

import argparse
import json
import tempfile
from pathlib import Path

import numpy as np
from facade_footprint import (
    ROOT,
    capture,
    masks,
    native_profiles,
    polygon_mask,
    prepare,
)
from PIL import Image, ImageDraw


def pixels():
    x, y = np.meshgrid(np.arange(160) - 80, np.arange(96) - 48)
    cases = [
        (1, np.array([0.055, 0.035]), np.array([-0.035, 0.055])),
        (8, np.array([0.23, 0.18]), np.array([-0.1, 0.26])),
        (1, np.array([0.055, 0]), np.array([0, 0.055])),
    ]
    values = []
    for style, dx, dy in cases:
        at = np.array([0.31, 0.47]) + x[..., None] * dx + y[..., None] * dy
        values.append(
            np.column_stack(
                [
                    at.reshape(-1, 2),
                    np.broadcast_to(dx, (x.size, 2)),
                    np.broadcast_to(dy, (x.size, 2)),
                    np.full(x.size, style),
                ]
            )
        )
    return np.concatenate(values).astype(np.float32).astype(float), cases


def reference(values, palette):
    coverage = masks(values, -1)
    band = np.zeros(len(values))
    for style in np.unique(values[:, 6]).astype(int):
        take = values[:, 6] == style
        fraction = min(
            0.16 * float(np.float32(1 / 2.9)), 1 - native_profiles()[style, 3]
        )
        band[take] = polygon_mask(
            values[take, :2] + [0, 0.5],
            values[take, 2:4],
            values[take, 4:6],
            np.array([0, (1 - fraction) / 2]),
            np.array([1, (1 + fraction) / 2]),
        )
    opening, pane = coverage.T
    colour = (1 - opening[:, None]) * palette[4:7] + (opening - pane)[:, None] * [
        0.48,
        0.46,
        0.41,
    ]
    colour += pane[:, None] * [0.025, 0.035, 0.045]
    return colour - 0.35 * band[:, None] * palette[4:7]


def image(values):
    linear = np.maximum(values, 0)
    srgb = np.where(
        linear <= 0.0031308, 12.92 * linear, 1.055 * linear ** (1 / 2.4) - 0.055
    )
    return Image.fromarray(np.uint8(np.clip(srgb, 0, 1) * 255)).resize((320, 192))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use a directory in system temp")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    values, cases = pixels()
    prepare(output, values)
    old, new = (
        capture(output, args.binary.resolve(), 0),
        capture(output, args.binary.resolve(), 5),
    )
    target = reference(values, np.fromfile(output / "palette.f32", dtype="<f4"))
    canvas = Image.new("RGB", (960, 672), "white")
    draw = ImageDraw.Draw(canvas)
    draw.text(
        (8, 4),
        "Actual native affine wall albedo; 160x96 pixels enlarged 2x. No lighting/Place/frame claim.",
        fill="black",
    )
    for row, (style, _, _) in enumerate(cases):
        for col, (label, colour) in enumerate(
            [
                ("old box", old[:, :3]),
                ("native nine subcells", new[:, :3]),
                ("independent polygon area", target),
            ]
        ):
            draw.text(
                (col * 320 + 4, row * 216 + 24), f"{label}, style {style}", fill="black"
            )
            start = row * 160 * 96
            canvas.paste(
                image(colour[start : start + 160 * 96].reshape(96, 160, 3)),
                (col * 320, row * 216 + 44),
            )
    canvas.save(output / "albedo.png")
    report = {
        "cases": len(values),
        "old_rgb_rms": float(np.sqrt(np.mean((old[:, :3] - target) ** 2))),
        "new_rgb_rms": float(np.sqrt(np.mean((new[:, :3] - target) ** 2))),
        "gpu_executed": True,
        "command_scope": __doc__,
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
