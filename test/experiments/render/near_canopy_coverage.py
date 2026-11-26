"""Compare captured tree coverage at the same orthographic world scale."""

import argparse
import json
from pathlib import Path

import numpy as np
from group_impostor_views import TEXEL, basis


def capture(folder):
    return json.loads((folder / "group.json").read_text())


def mask(folder, view, side):
    return np.fromfile(folder / f"{view}.field", TEXEL)["surface"].reshape(side, side) != 0


def compare(reference, candidate):
    source, reduced = (capture(folder) for folder in (reference, candidate))
    side = source["pixels"]
    if (side < 64 or side != reduced["pixels"] or side % 64 != 0
            or not np.allclose(source["directions"], reduced["directions"])):
        raise ValueError("aligned capture directions and resolution required")
    block = side // 64
    t = (np.arange(side) + .5) * (2 / side) - 1
    coords = np.stack(np.meshgrid(t, -t), axis=-1)
    delta = np.asarray(source["centre_m"]) - reduced["centre_m"]
    rows = []
    for view, axis in enumerate(source["directions"]):
        translation = delta @ basis(np.asarray(axis))[:, :2]
        uv = (coords * source["half_extent_m"] + translation) / reduced["half_extent_m"]
        ij = np.floor((uv * [1, -1] + 1) * (.5 * side)).astype(int)
        inside = ((ij >= 0) & (ij < side)).all(axis=-1)
        x, y = np.clip(ij, 0, side - 1).transpose(2, 0, 1)
        active = mask(candidate, view, side)[y, x] & inside
        guess = active.reshape(64, block, 64, block).mean(axis=(1, 3))
        target = mask(reference, view, side).reshape(64, block, 64, block).mean(axis=(1, 3))
        union = np.maximum(guess, target).sum()
        rows.append(float(np.minimum(guess, target).sum() / union) if union else 1.)
    return {"triangles": reduced["source_triangles"], "views": len(rows),
            "median_coverage_iou": float(np.median(rows)), "minimum_coverage_iou": min(rows)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    options = parser.parse_args()
    print(json.dumps(compare(options.reference, options.candidate)))


if __name__ == "__main__":
    main()
