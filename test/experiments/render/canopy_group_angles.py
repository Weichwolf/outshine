"""Measure angular sampling of the forest stand at equal card resolution."""

import argparse
from functools import reduce
from pathlib import Path

import numpy as np

from group_impostor_views import (HORIZONTAL, PROBES, UPPER, basis, direction, fields,
                                  foreground, hierarchical, preview, raster_card, reduce_field,
                                  statistics)


def ring(elevation, offset):
    return tuple((azimuth + offset, elevation) for azimuth in range(0, 360, 45))


BASE = HORIZONTAL + UPPER
MEDIUM = BASE + ring(6., 22.5) + ring(35., 22.5)
DENSE = MEDIUM + ring(18., 0.) + ring(62., 0.) + ring(72., 22.5)
ANGLES = DENSE + PROBES
METRICS = ("coverage_iou", "missing_fraction", "extra_fraction", "shared_depth_p95_m")


def compare_angle(maps, vectors, frames, view, side, half_extent):
    target = raster_card(maps[view], frames[view], frames[view], side)
    previews = [target]
    rows = {}
    for layout in (BASE, MEDIUM, DENSE):
        order = np.argsort(-(vectors[:len(layout)] @ vectors[view]), kind="stable")[:4]
        cards = [raster_card(maps[j], frames[j], frames[view], side, tangent=True) for j in order]
        for layers in (2, 4):
            projected = reduce(foreground, cards[:layers])
            metrics = statistics(projected, target, half_extent)
            rows[len(layout), layers] = [metrics[key] for key in METRICS]
            if layers == 2:
                previews.append(projected)
    return rows, previews


def evaluate(root, output):
    manifest, vectors, data = fields(root, ANGLES)
    frames = [basis(v) for v in vectors]
    print("side views drawn " + " ".join(METRICS))
    for block in (2, 4):
        side = manifest["pixels"] // block
        maps = [reduce_field(field, block) for field in data]
        scores = {}
        for view in range(len(DENSE), len(ANGLES)):
            rows, previews = compare_angle(maps, vectors, frames, view, side,
                                           manifest["half_extent_m"])
            for key, values in rows.items():
                scores.setdefault(key, []).append(values)
            if block == 2 and ANGLES[view] in ((51, 28), (147, 85)):
                azimuth, altitude = ANGLES[view]
                preview(previews, output / f"angles-{azimuth}-{altitude}.png")
        for key, samples in scores.items():
            medians = [f"{value:.3f}" for value in np.median(samples, axis=0)]
            print(side, *key, *medians)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest="mode", required=True)
    modes.add_parser("directions")
    comparison = modes.add_parser("evaluate")
    comparison.add_argument("capture", type=Path)
    comparison.add_argument("output", type=Path)
    composition = modes.add_parser("compose")
    composition.add_argument("tree", type=Path)
    composition.add_argument("stand", type=Path)
    composition.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.mode == "directions":
        for angle in ANGLES:
            print(*direction(*angle))
        return
    args.output.mkdir()
    if args.mode == "evaluate":
        evaluate(args.capture, args.output)
    else:
        hierarchical(args.tree, args.stand, args.output, ANGLES)


if __name__ == "__main__":
    main()
