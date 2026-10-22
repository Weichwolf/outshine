"""Compare exact rectangle unions on an explicit captured receiver trace."""

import argparse
import hashlib
import json
import struct
import time
from pathlib import Path


def scalar(rectangles, side):
    grid = bytearray(side * side)
    for left, right, top, bottom in rectangles:
        for y in range(top, bottom + 1):
            for x in range(left, right + 1):
                grid[y * side + x] = 1
    return bytes(grid)


def rows(rectangles, side):
    grid = [0] * side
    for left, right, top, bottom in rectangles:
        mask = ((1 << (right - left + 1)) - 1) << left
        for y in range(top, bottom + 1):
            grid[y] |= mask
    return bytes((row >> x) & 1 for row in grid for x in range(side))


def difference(rectangles, side):
    stride = side + 1
    grid = [0] * (stride * stride)
    for left, right, top, bottom in rectangles:
        grid[top * stride + left] += 1
        grid[top * stride + right + 1] -= 1
        grid[(bottom + 1) * stride + left] -= 1
        grid[(bottom + 1) * stride + right + 1] += 1
    result = bytearray(side * side)
    for y in range(side):
        row_sum = 0
        for x in range(side):
            index = y * stride + x
            row_sum += grid[index]
            grid[index] = row_sum + (grid[index - stride] if y else 0)
            result[y * side + x] = grid[index] > 0
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace", required=True, type=Path)
    parser.add_argument("--side", required=True, type=int)
    parser.add_argument("--limit", default=50000, type=int)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if not 0 < args.side <= 1024 or args.limit <= 0:
        parser.error("side must be in [1,1024] and limit must be positive")
    raw = args.trace.read_bytes()
    if not raw or len(raw) % 16:
        parser.error("expected little-endian uint32 inclusive rectangles")
    rectangles = list(struct.iter_unpack("<4I", raw[: args.limit * 16]))
    if any(
        not (left <= right < args.side and top <= bottom < args.side)
        for left, right, top, bottom in rectangles
    ):
        parser.error("rectangle outside profile")
    outputs, measurements = [], []
    for algorithm in (scalar, rows, difference):
        start = time.perf_counter()
        mask = algorithm(rectangles, args.side)
        seconds = time.perf_counter() - start
        outputs.append(mask)
        measurements.append(
            {
                "algorithm": algorithm.__name__,
                "seconds": seconds,
                "mask_sha256": hashlib.sha256(mask).hexdigest(),
            }
        )
    if not all(mask == outputs[0] for mask in outputs):
        raise ValueError("rectangle union mismatch")
    report = {
        "trace_sha256": hashlib.sha256(raw).hexdigest(),
        "trace_rectangles": len(raw) // 16,
        "tested_rectangles": len(rectangles),
        "side": args.side,
        "scalar_cell_updates": sum(
            (r - l + 1) * (b - t + 1) for l, r, t, b in rectangles
        ),
        "row_word_updates": sum(
            (r // 64 - l // 64 + 1) * (b - t + 1) for l, r, t, b in rectangles
        ),
        "difference_corner_updates": 4 * len(rectangles),
        "difference_scan_cells": args.side * args.side,
        "marked_cells": sum(outputs[0]),
        "measurements": measurements,
        "success": True,
        "scope": "Finite captured rectangle-union comparison; Python times are not native device times, no geometric/raster qualification or page suppression",
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
