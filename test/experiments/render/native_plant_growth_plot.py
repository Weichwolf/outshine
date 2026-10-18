"""Two-view orthographic skeleton diagnostic; not a game-rendering acceptance."""

import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "vegetation"))
from oak_scaffold import tapered_radii


def load(path):
    lines = path.read_text().splitlines()
    count = int(lines[0].split()[1])
    parents = np.array([int(line.split()[1]) for line in lines[1 : count + 1]])
    points = np.array(
        [list(map(float, line.split()[2:])) for line in lines[1 : count + 1]]
    )
    return points, parents


def draw_panel(draw, points, parents, *, view, column, row):
    view = np.asarray(view, dtype=float)
    view /= np.linalg.norm(view)
    right = np.cross(view, [0, 0, 1])
    right /= np.linalg.norm(right)
    up = np.cross(right, view)
    delta = points - [0, 0, 9]
    factor = (900 - 70) / 21
    xy = np.c_[delta @ right, -(delta @ up)] * factor
    xy += [column * 800 + 400, row * 900 + 465]
    xy *= 2
    radii = tapered_radii(parents, 0.325, 0.0015)
    for child in np.argsort(delta @ view)[::-1]:
        if child == 0:
            continue
        parent = parents[child]
        width = max(1, round((radii[parent] + radii[child]) * factor * 2))
        draw.line([tuple(xy[parent]), tuple(xy[child])], fill=(83, 70, 50), width=width)
    label = "6 primary limbs" if column else "trunk only"
    draw.text(
        ((column * 800 + 25) * 2, (row * 900 + 15) * 2),
        f"Native growth, seed682: {label} / {len(parents)}nodes",
        fill=(15, 30, 20),
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--inputs", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    image = Image.new("RGB", (3200, 3600), (235, 239, 235))
    draw = ImageDraw.Draw(image)
    for row, view in enumerate(((0, -1, 0.1), (1, -1, 0.35))):
        for column in (0, 1):
            points, parents = load(args.inputs / f"oak-682-limbs-{column}.native")
            draw_panel(draw, points, parents, view=view, column=column, row=row)
    image.resize((1600, 1800), Image.Resampling.LANCZOS).save(args.output)


if __name__ == "__main__":
    main()
