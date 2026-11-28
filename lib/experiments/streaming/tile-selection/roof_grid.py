"""Measure aligned roof-height grids against the same source footprints."""

import argparse
import json
import math
from itertools import groupby
from pathlib import Path
from time import perf_counter

import numpy as np
from group_footprints import (
    Extrusion,
    measure,
    read_source,
    sample_distances,
    triangles,
)
from shapely import contains_xy, simplify
from shapely.geometry import box


def grid_ranges(bounds, pitch):
    return tuple(math.ceil(bound / pitch - 0.5) for bound in bounds)


def grid_extrusions(source, pitch, height_step):
    bottomed = [part for part in source if part.bottom_m == 0]
    spans = np.asarray([grid_ranges(part.footprint.bounds, pitch) for part in bottomed])
    x0, y0 = spans[:, :2].min(axis=0)
    x1, y1 = spans[:, 2:].max(axis=0)
    roof = np.zeros((y1 - y0, x1 - x0), dtype=np.float32)
    for part, (left, top, right, bottom) in zip(bottomed, spans):
        if left >= right or top >= bottom:
            continue
        x = (np.arange(left, right)[None, :] + 0.5) * pitch
        z = (np.arange(top, bottom)[:, None] + 0.5) * pitch
        occupied = contains_xy(part.footprint, x, z)
        region = roof[top - y0 : bottom - y0, left - x0 : right - x0]
        height = max(height_step, round(part.top_m / height_step) * height_step)
        np.maximum(region, height, out=region, where=occupied)
    bands = {}
    for y, heights in enumerate(roof, y0):
        columns = int(x0)
        next_bands = {}
        for height, values in groupby(heights.tolist()):
            length = sum(1 for _ in values)
            if height:
                span = (columns, columns + length, height)
                next_bands[span] = bands.pop(span, y)
            columns += length
        for (left, right, height), top in bands.items():
            yield Extrusion(
                box(left * pitch, top * pitch, right * pitch, y * pitch), 0, height
            )
        bands = next_bands
    for (left, right, height), top in bands.items():
        yield Extrusion(
            box(left * pitch, top * pitch, right * pitch, y1 * pitch), 0, height
        )
    for part in source:
        if part.bottom_m != 0:
            yield Extrusion(
                simplify(part.footprint, pitch / 4, preserve_topology=True),
                part.bottom_m,
                part.top_m,
            )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--pitch-m", type=float, nargs="+", default=(4.0, 8.0, 16.0))
    options = parser.parse_args()
    if any(pitch <= 0 or not math.isfinite(pitch) for pitch in options.pitch_m):
        parser.error("expected positive finite grid spacings")

    source = tuple(read_source(options.source))
    if not any(part.bottom_m == 0 for part in source):
        parser.error("the sample needs grounded source geometry")
    variants, times = {}, {}
    for pitch in options.pitch_m:
        started = perf_counter()
        variants[pitch] = tuple(grid_extrusions(source, pitch, 4))
        times[pitch] = (perf_counter() - started) * 1000
    report = measure(
        source, variants, sample_distances(source, variants), options.image
    )
    print(
        json.dumps(
            {
                "reference_triangles": triangles(source),
                "variants": {
                    pitch: {
                        "triangles": triangles(value),
                        "build_ms": times[pitch],
                        "views": report[pitch],
                    }
                    for pitch, value in variants.items()
                },
            }
        )
    )


if __name__ == "__main__":
    main()
