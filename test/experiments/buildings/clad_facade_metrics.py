"""Actual RGB-byte error globally and inside independently projected building bounds."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


def pixels(path):
    with Image.open(path) as image:
        return np.asarray(image.convert("RGB"), dtype=np.int16)


def bounds(root, index):
    name = f"{index}Farfull"
    render = json.loads((root / "renders" / (name + "-render.json")).read_text())
    data = json.loads((root / (name + "-geometry.json")).read_text())
    with np.load(data["mesh"]) as mesh:
        v = np.c_[mesh["vertices"], np.ones(len(mesh["vertices"]))]
    c = render["camera"]
    clip = v @ np.asarray(c["view_matrix"]).T @ np.asarray(c["projection_matrix"]).T
    if np.any(clip[:, 3] <= 0):
        raise ValueError("Finite far comparison requires every building corner ahead")
    w, h = render["profile"]
    p = (clip[:, :2] / clip[:, 3:4] * [1, -1] + 1) * [w / 2, h / 2]
    lo = np.maximum(np.floor(p.min(axis=0)).astype(int) - 1, 0)
    hi = np.minimum(np.ceil(p.max(axis=0)).astype(int) + 1, [w - 1, h - 1])
    return [int(lo[0]), int(lo[1]), int(hi[0]), int(hi[1])]


def delta(reference, image, bbox=None):
    d = abs(reference - image)
    if bbox is not None:
        x0, y0, x1, y1 = bbox
        d = d[y0 : y1 + 1, x0 : x1 + 1]
    return {
        "mean_rgb_error_255": float(d.mean()),
        "worst_rgb_error_255": int(d.max()),
        "different_pixels": int(np.any(d, axis=2).sum()),
        "pixel_count": d.shape[0] * d.shape[1],
    }


def analyse(roots, negative, output):
    records = []
    for root in roots:
        for file in sorted(root.glob("*Farfull-geometry.json")):
            index = file.name.removesuffix("Farfull-geometry.json")
            bbox = bounds(root, index)
            full = pixels(root / "renders" / f"{index}Farfull.png")
            values = {}
            for method in ("front", "omit"):
                path = root / "renders" / f"{index}Far{method}.png"
                image = pixels(path)
                values[method] = {
                    "global": delta(full, image),
                    "projected_building_bbox": delta(full, image, bbox),
                    "image_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                }
            records.append(
                {
                    "case": index,
                    "root": str(root),
                    "bbox_inclusive": bbox,
                    "entries": values,
                }
            )
    if negative is not None:
        for file in sorted(negative.glob("*NearControlfull-geometry.json")):
            index = file.name.removesuffix("NearControlfull-geometry.json")
            full, front = [
                pixels(negative / "renders" / f"{index}NearControl{mode}.png")
                for mode in ("full", "front")
            ]
            records.append(
                {"near_counter_case": index, "forced_front": delta(full, front)}
            )
    output.write_text(
        json.dumps(
            {
                "records": records,
                "scope": "Actual8-bit RGB errors, not linear radiance. ROI is actual full-building mesh projected by saved Blender camera matrices, expanded by one raster pixel and clipped to viewport; independent of observed error. Global comparison retains background/noise/shadow differences. Limited fixed daylight/cameras/samples, not a universal night/shadow/grazing/temporal LOD bound.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--models", type=Path, nargs="+", required=True)
    parser.add_argument("--negative", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    analyse(args.models, args.negative, args.output)
