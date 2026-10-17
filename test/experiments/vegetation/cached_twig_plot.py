"""Physical/window/error projections at a declared shared direction, never a lit showcase."""

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


def rgb(image):
    colour = image[..., :3] + (1 - image[..., 3, None]) * 0.94
    colour = np.where(
        colour <= 0.0031308,
        12.92 * colour,
        1.055 * np.maximum(colour, 0) ** (1 / 2.4) - 0.055,
    )
    return np.uint8(np.rint(np.clip(np.flipud(colour), 0, 1) * 255))


def plot(directory, output, view=8):
    report = json.loads((directory / "comparison.json").read_text())
    canvas = Image.new("RGB", (2560, 1440), "white")
    draw = ImageDraw.Draw(canvas)
    draw.text(
        (24, 12),
        f"Picea abies | actual physical / statistical windows / alpha error | shared view {view}/16 | unlit",
        fill="black",
    )
    for i, case in enumerate(report["cases"]):
        with np.load(
            directory / str(case["axis_id"]) / "views.npz", allow_pickle=False
        ) as f:
            a, b = f["physical"][view], f["windows"][view]
        delta = b[..., 3] - a[..., 3]
        error = np.zeros((*delta.shape, 3))
        error[..., 0] = np.maximum(-delta, 0)
        error[..., 2] = np.maximum(delta, 0)
        images = (rgb(a), rgb(b), np.uint8(np.rint(np.flipud(error) * 255)))
        strip = Image.fromarray(np.concatenate(images, axis=1)).resize(
            (810, 270), Image.Resampling.NEAREST
        )
        x, y = 22 + (i % 3) * 850, 62 + (i // 3) * 440
        canvas.paste(strip, (x, y))
        metric = case["views"][view]["needle"]
        draw.text(
            (x, y + 282),
            f"order {case['order']} | axis {case['axis_id']} | {case['needles']} needles | tail {case['tail_weight']:.3f}",
            fill="black",
        )
        draw.text(
            (x, y + 306),
            f"needle area error {metric['relative_area_error']:+.1%} | L1 at4px {metric['l1_by_size']['4']:.3f}",
            fill="black",
        )
    draw.text(
        (24, 1406),
        "Red: missing alpha; blue: extra alpha. Original128square projections enlarged, no lighting/shadow/native/LOD approval.",
        fill="black",
    )
    canvas.save(output)


def plot_holdout(directory, output, view=8):
    report = json.loads((directory / "holdout.json").read_text())
    canvas = Image.new("RGB", (2560, 1440), "white")
    draw = ImageDraw.Draw(canvas)
    draw.text(
        (24, 12),
        f"Actual isolated tail windows | physical / original / size matched | shared view {view}/16 | alpha in sRGB grayscale",
        fill="black",
    )
    for i, case in enumerate(report["cases"]):
        with np.load(
            directory / f"{case['window_index']}.npz", allow_pickle=False
        ) as f:
            images = f["images"][view]
        pixels = []
        for image in images:
            alpha = image[..., 3]
            value = np.where(
                alpha <= 0.0031308,
                alpha * 12.92,
                1.055 * np.maximum(alpha, 0) ** (1 / 2.4) - 0.055,
            )
            pixels.append(
                np.repeat(
                    np.uint8(np.rint(np.flipud(np.clip(value, 0, 1)) * 255))[..., None],
                    3,
                    axis=-1,
                )
            )
        strip = Image.fromarray(np.concatenate(pixels, axis=1)).resize(
            (810, 270), Image.Resampling.NEAREST
        )
        x, y = 22 + (i % 3) * 850, 62 + (i // 3) * 440
        canvas.paste(strip, (x, y))
        a = case["views"][view]["original"]["needle"]
        b = case["views"][view]["adaptive"]["needle"]
        draw.text(
            (x, y + 282),
            f"axis {case['axis_id']} | {case['needles']} actual needles | camera span {max(case['camera_span_px']):.2f}px",
            fill="black",
        )
        draw.text(
            (x, y + 306),
            f"L1 at4px original {a['l1_by_size']['4']:.3f} / matched {b['l1_by_size']['4']:.3f}",
            fill="black",
        )
    draw.text(
        (24, 1406),
        "Original128square projections enlarged; absolute alpha, not lighting or botanical/native/LOD approval.",
        fill="black",
    )
    canvas.save(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--comparison", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--view", type=int, default=8)
    parser.add_argument("--holdout", action="store_true")
    args = parser.parse_args()
    (plot_holdout if args.holdout else plot)(args.comparison, args.output, args.view)
