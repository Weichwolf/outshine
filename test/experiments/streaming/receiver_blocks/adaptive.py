"""Compare adaptive outer receiver blocks with all scalar points of one capture."""

import argparse
import hashlib
import itertools
import json
import math
import time
from pathlib import Path

import correlated
import prepared
from interval import project
from probe import depth, scalar_point


def rectangle(bounds, region, atlas):
    (x0, x1), (y0, y1), (z0, z1) = bounds
    if x0 > 1 or x1 < -1 or y0 > 1 or y1 < -1 or z0 > 1 or z1 < 0:
        return None
    side = atlas // 2
    px = lambda x: (min(1, max(-1, x)) + 1) * 0.5 * side
    py = lambda y: (1 - min(1, max(-1, y))) * 0.5 * side
    ax, ay = (int(region["atlas"][i] * atlas) for i in (0, 1))
    return (
        (max(0, math.floor(px(x0)) - 8) + ax) // 16,
        (min(side - 1, math.ceil(px(x1)) + 8) + ax) // 16,
        (max(0, math.floor(py(y1)) - 8) + ay) // 16,
        (min(side - 1, math.ceil(py(y0)) + 8) + ay) // 16,
    )


def mark(rows, box):
    if box is None:
        return
    left, right, top, bottom = box
    mask = ((1 << (right - left + 1)) - 1) << left
    for y in range(top, bottom + 1):
        rows[y] |= mask


def scalar(values, frame):
    rows = [0] * (frame["shadow"]["side"] // 16)
    width = frame["width"]
    count = 0
    for index, value in enumerate(values):
        if not math.isfinite(value) or not 0 < value <= 1:
            continue
        points = scalar_point(index % width, index // width, value, frame)
        if points is None:
            continue
        count += 1
        for point, region in zip(points, frame["shadow"]["regions"], strict=True):
            mark(
                rows,
                rectangle([(v, v) for v in point], region, frame["shadow"]["side"]),
            )
    return rows, count


def adaptive(values, frame, tile, maximum_cells, projection="prepared"):
    width, height = frame["width"], frame["height"]
    atlas = frame["shadow"]["side"]
    rows = [0] * (atlas // 16)
    regions = frame["shadow"]["regions"]
    lights = [r["projection"] for r in regions]
    frame_reads = 0
    if projection == "prepared":
        minimum, maximum = math.inf, -math.inf
        for value in values:
            frame_reads += 1
            if math.isfinite(value) and 0 < value <= 1:
                minimum, maximum = min(minimum, value), max(maximum, value)
        error_box = (
            [(-1.0, 1.0), (-1.0, 1.0), (1 - 2 * maximum, 1 - 2 * minimum)]
            if minimum != math.inf
            else None
        )
        projector = prepared.Prepared(
            frame["inverse_camera_clip"], frame["eye"], lights, error_box
        )
    else:
        method = correlated.project if projection == "correlated" else project
        projector = lambda box: method(
            box, frame["inverse_camera_clip"], frame["eye"], lights
        )
    work = {
        "nodes": 0,
        "frame_depth_reads": frame_reads,
        "global_error_prepared": projection == "prepared"
        and projector.frame_error is not None,
        "depth_reads": 0,
        "empty_nodes": 0,
        "unknown_nodes": 0,
        "refined_nodes": 0,
        "emitted_nodes": 0,
        "point_leaves": 0,
    }

    def walk(left, right, top, bottom):
        work["nodes"] += 1
        minimum, maximum = math.inf, -math.inf
        for y in range(top, bottom):
            for x in range(left, right):
                work["depth_reads"] += 1
                value = values[y * width + x]
                if math.isfinite(value) and 0 < value <= 1:
                    minimum, maximum = min(minimum, value), max(maximum, value)
        if minimum == math.inf:
            work["empty_nodes"] += 1
            return
        if right - left == bottom - top == 1:
            work["emitted_nodes"] += 1
            work["point_leaves"] += 1
            points = scalar_point(left, top, minimum, frame)
            if points is None:
                work["unknown_nodes"] += 1
                mark(rows, (0, len(rows) - 1, 0, len(rows) - 1))
            else:
                for point, region in zip(points, regions, strict=True):
                    mark(rows, rectangle([(v, v) for v in point], region, atlas))
            return
        ndc = [
            (2 * (left + 0.5) / width - 1, 2 * (right - 0.5) / width - 1),
            (1 - 2 * (bottom - 0.5) / height, 1 - 2 * (top + 0.5) / height),
            (1 - 2 * maximum, 1 - 2 * minimum),
        ]
        try:
            bounds = projector(ndc)
            boxes = [
                rectangle(b, r, atlas) for b, r in zip(bounds, regions, strict=True)
            ]
        except ArithmeticError:
            work["unknown_nodes"] += 1
            boxes = [(0, len(rows) - 1, 0, len(rows) - 1)]
        area = max(
            (
                (r - l + 1) * (b - t + 1)
                for box in boxes
                if box is not None
                for l, r, t, b in [box]
            ),
            default=0,
        )
        if area > maximum_cells and (right - left > 1 or bottom - top > 1):
            work["refined_nodes"] += 1
            xs = (
                (left, (left + right) // 2, right)
                if right - left > 1
                else (left, right)
            )
            ys = (
                (top, (top + bottom) // 2, bottom)
                if bottom - top > 1
                else (top, bottom)
            )
            for y0, y1 in itertools.pairwise(ys):
                for x0, x1 in itertools.pairwise(xs):
                    walk(x0, x1, y0, y1)
            return
        work["emitted_nodes"] += 1
        work["point_leaves"] += right - left == bottom - top == 1
        for box in boxes:
            mark(rows, box)

    for top in range(0, height, tile):
        for left in range(0, width, tile):
            walk(left, min(width, left + tile), top, min(height, top + tile))
    return rows, work


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frame", type=Path, required=True)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--tile", type=int, default=16)
    parser.add_argument("--max-cells", type=int, default=16)
    parser.add_argument(
        "--projection",
        choices=("cartesian", "correlated", "prepared"),
        default="prepared",
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference", type=Path)
    args = parser.parse_args()
    frame = json.loads(args.frame.read_text())
    values = depth(args.captures / frame["depth"], frame["width"] * frame["height"])
    if args.reference is None:
        start = time.perf_counter()
        actual, samples = scalar(values, frame)
        scalar_seconds = time.perf_counter() - start
    else:
        descriptor = json.loads(args.reference.read_text())
        if (
            descriptor["frame_sha256"]
            != hashlib.sha256(args.frame.read_bytes()).hexdigest()
        ):
            raise ValueError("reference frame mismatch")
        if (
            descriptor["depth_sha256"]
            != hashlib.sha256((args.captures / frame["depth"]).read_bytes()).hexdigest()
        ):
            raise ValueError("reference depth mismatch")
        raw = (args.reference.parent / descriptor["mask_file"]).read_bytes()
        if descriptor["mask_sha256"] != hashlib.sha256(raw).hexdigest():
            raise ValueError("reference mask mismatch")
        side = frame["shadow"]["side"] // 16
        if len(raw) != side * side or any(value not in (0, 1) for value in raw):
            raise ValueError("invalid reference mask")
        actual = [sum(raw[y * side + x] << x for x in range(side)) for y in range(side)]
        samples = descriptor["scalar_samples"]
        scalar_seconds = None
    start = time.perf_counter()
    outer, work = adaptive(
        values,
        frame,
        args.tile,
        args.max_cells,
        args.projection,
    )
    block_seconds = time.perf_counter() - start
    missing = sum((a & ~b).bit_count() for a, b in zip(actual, outer, strict=True))
    extra = sum((b & ~a).bit_count() for a, b in zip(actual, outer, strict=True))
    result = {
        "frame_sha256": hashlib.sha256(args.frame.read_bytes()).hexdigest(),
        "tile": args.tile,
        "max_cells": args.max_cells,
        "projection": args.projection,
        "scalar_samples": samples,
        "scalar_marked_cells": sum(r.bit_count() for r in actual),
        "block_marked_cells": sum(r.bit_count() for r in outer),
        "missing_cells": missing,
        "extra_cells": extra,
        "scalar_seconds": scalar_seconds,
        "block_seconds": block_seconds,
        "work": work,
        "success": missing == 0,
        "qualified_for_page_suppression": False,
        "scope": "One-frame finite Python receiver superset; no native algorithm timings, side-consumer completeness or world admission",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    if missing:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
